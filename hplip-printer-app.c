//
// HPLIP (hpcups) Printer Application based on PAPPL and libpappl-retrofit
//
// Copyright © 2020-2021 by Till Kamppeter.
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#define _GNU_SOURCE

#include <pappl-retrofit.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

//
// Constants...
//

// Name and version

#define SYSTEM_NAME "HPLIP Printer Application"
#define SYSTEM_PACKAGE_NAME "hplip-printer-app"
#ifndef SYSTEM_VERSION_STR
#  define SYSTEM_VERSION_STR "1.0"
#endif
#ifndef SYSTEM_VERSION_ARR_0
#  define SYSTEM_VERSION_ARR_0 1
#endif
#ifndef SYSTEM_VERSION_ARR_1
#  define SYSTEM_VERSION_ARR_1 0
#endif
#ifndef SYSTEM_VERSION_ARR_2
#  define SYSTEM_VERSION_ARR_2 0
#endif
#ifndef SYSTEM_VERSION_ARR_3
#  define SYSTEM_VERSION_ARR_3 0
#endif
#define SYSTEM_WEB_IF_FOOTER "Copyright &copy; 2021 by Till Kamppeter. Provided under the terms of the <a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>."

// Test page

#define TESTPAGE "testpage.pdf"

// System architecture

#if defined(__x86_64__) || defined(_M_X64)
  #define ARCH "x86_64"
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
  #define ARCH "x86_32"
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_ARCH_ISA_A64)
  #define ARCH "arm64"
#elif defined(__arm__) || defined(_M_ARM)
  #define ARCH "arm32"
#else
  // "grep"ping the binary should not match this
  #define ARCH "UNSUPPORTED_ARCH"
#endif

// Plugin download URLs

#define PLUGIN_CONF_URL "http://hplip.sf.net/plugin.conf"
#define PLUGIN_ALT_LOCATION "https://developers.hp.com/sites/default/files"


//
// Types...
//

typedef enum hplip_plugin_status_e
{
  HPLIP_PLUGIN_NOT_INSTALLED = 0,
  HPLIP_PLUGIN_OUTDATED,
  HPLIP_PLUGIN_INSTALLED
} hplip_plugin_status_t;


//
// Functions...
//

//
// 'get_config_value()' - Return the value of a given variable/key in
//                        a given section of a config file
//

char *
get_config_value(FILE *fp,
		 const char *section,
		 const char *key)
{
  char line[1024];
  char *value = NULL;
  int in_section = 0;

  if (!key || !key[0])
    return (NULL);

  rewind(fp);

  while (fgets(line, sizeof(line), fp))
  {
    while (line[strlen(line) - 1] == '\n' ||
	   line[strlen(line) - 1] == '\r') // Remove newline
      line[strlen(line) - 1] = '\0';

    if (line[0] == '[')
    {
      if (section && !strncasecmp(line + 1, section, strlen(section)) &&
	  line[strlen(section) + 1] == ']')
	in_section = 1;
      else
	in_section = 0;
    }
    else if ((!section || in_section) &&
	     !strncasecmp(line, key, strlen(key)))
    {
      value = line + strlen(key);
      while (*value && isspace(*value)) value ++;
      if (*value == '=')
      {
	value ++;
	while (*value && isspace(*value)) value ++;
	if (*value)
	{
	  value = strdup(value);
	  break;
	}
	else
	  value = NULL;
      }
      else
	value = NULL;
    }
  }

  return (value);
}


//
// 'set_config_value()' - Set a new value for a given variable/key in
//                        a given section of a config file. Create the
//                        section/key if not yet present.
//

int
set_config_value(char **filebuf,
		 const char *section,
		 const char *key,
		 const char *value)
{
  char line[1024];
  int section_found = 0,
      line_changed = 0,
      file_changed = 0,
      done = 0;
  char *filebufptr, *buf, *bufptr, *lineptr, *lineendptr;
  size_t size_needed,
         bytes_written;

  if (!filebuf || !key || !key[0])
    return (0);

  size_needed = (*filebuf ? strlen(*filebuf) : 0) +
                (section ? strlen(section) : 0) +
                strlen(key) +
                (value ? strlen(value) : 0) + 8;

  // Create a buffer for the whole file plus the new entry
  buf = (char *)calloc(size_needed, sizeof(char));
  bufptr = buf;

  // Read the lines of the file to find the point where to apply the change
  // and put the lines including the change into the buffer
  if (*filebuf)
  {
    filebufptr = *filebuf;
    while (*filebufptr)
    {
      // Read one line from input buffer
      for (line[0] = '\0', lineptr = line;
	   *filebufptr && *filebufptr != '\n';
	   filebufptr++, lineptr ++)
	*lineptr = *filebufptr;
      *lineptr = '\n';
      lineptr ++;
      *lineptr = '\0';
      if (*filebufptr) filebufptr ++;
      line_changed = 0;

      if (strlen(line) > 0 && !done)
      {
	// We have not yet set the requested value, still searching the
	// right place in the file ...
	if (line[0] == '[')
        {
	  // New section
	  if (section && !strncasecmp(line + 1, section, strlen(section)) &&
	      line[strlen(section) + 1] == ']')
	    // Start of requested section
	    section_found = 1;
	  else if (section_found && !done && value)
	  {
	    // Requested section has ended and new setting not yet written,
	    // Create new line at end of section
	    bytes_written = sprintf(bufptr, "%s = %s\n", key, value);
	    bufptr += bytes_written;
	    file_changed = 1;
	    done = 1;
	  }
	}
	else if ((!section || section_found) &&
	       !strncasecmp(line, key, strlen(key)))
        {
	  // Line begins with the name of the key we want to change, continue
	  // parsing
	  lineptr = line + strlen(key);
	  while (*lineptr && isspace(*lineptr)) lineptr ++;
	  if (*lineptr == '=' || *lineptr == '\n' || *lineptr == '\r' ||
	      *lineptr == '\0')
	  {
	    // The line actually sets our key, right after the key name is
	    // an '=' ot the line end
	    if (value) // value == NULL removes the key in the section
	    {
	      // We have a valiue, so we change the key's value to hours
	      if (*lineptr == '=')
	      {
		// Find start of value in line
		lineptr ++;
		while (*lineptr && isspace(*lineptr)) lineptr ++;
	      }
	      // Find end of value in line
	      lineendptr = lineptr;
	      while (*lineendptr && *lineendptr != '\n' && *lineendptr != '\r')
		lineendptr ++;
	      if (strlen(value) != lineendptr - lineptr ||
		  strncmp(value, lineptr, lineendptr - lineptr))
	      {
		// Value differs from the current one, only then write the
		// line with the value replaced by hours
		sprintf(bufptr, "%s", line);
		bufptr += (lineptr - line);
		bytes_written = sprintf(bufptr, "%s%s", value, lineendptr);
		bufptr += bytes_written;
		line_changed = 1;
		file_changed = 1;
	      }
	    }
	    else
	    {
	      // Value is NULL, meaning that we want to remove the key, so
	      // mark the line as changed without writing it
	      line_changed = 1;
	      file_changed = 1;
	    }
	    done = 1;
	  }
	}
      }
      if (!line_changed)
      {
	// No change needed on original line, write it as it is
	bytes_written = sprintf(bufptr, "%s", line);
	bufptr += bytes_written;
      }
    }
  }

  if (!done && value)
  {
    // Requested section or requested key in section not found, create
    // the section and the line in it
    if (section && !section_found)
    {
      // Write section line
      bytes_written = sprintf(bufptr, "[%s]\n", section);
      bufptr += bytes_written;
    }
    // Write key=value line
    bytes_written = sprintf(bufptr, "%s = %s\n", key, value);
    bufptr += bytes_written;
    file_changed = 1;
  }
  *bufptr = '\0';

  if (file_changed)
  {
    // File has changed, replace the input buffer by the output buffer
    if (*filebuf)
      free(*filebuf);
    *filebuf = buf;
  }
  else
    free(buf);

  return (file_changed);
}


//
// 'hplip_version()' - Read out the HPLIP version from /etc/hp/hplip.conf
//

char *
hplip_version(pappl_system_t *system)
{
  char buf[1024];
  FILE *fp;
  char *version = NULL;


  snprintf(buf, sizeof(buf), "%s/%s", HPLIP_CONF_DIR, "hplip.conf");
  if ((fp = fopen(buf, "r")) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to open HPLIP configuration file %s", buf);
    return (NULL);
  }

  version = get_config_value(fp, "hplip", "version");

  fclose(fp);

  return (version);
}


//
// 'hplip_plugin_status()' - Read out the status of the installed p;lugin
//                           from /var/lib/hp/hplip.state
//

hplip_plugin_status_t
hplip_plugin_status(pappl_system_t *system)
{
  char buf[1024];
  FILE *fp;
  char *plugin_version,
       *installed_status,
       *version;
  hplip_plugin_status_t status = HPLIP_PLUGIN_NOT_INSTALLED;


  snprintf(buf, sizeof(buf), "%s/%s", HPLIP_PLUGIN_STATE_DIR, "hplip.state");
  if ((fp = fopen(buf, "r")) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to open HPLIP plugin status file %s", buf);
    return (status);
  }

  // HACK FOR TESTING: If empty hplip.state is created, install the plugin
  // also if it was not installed before
  fseek(fp, 0L, SEEK_END);
  if (ftell(fp) == 0)
  {
    fclose(fp);
    return (HPLIP_PLUGIN_OUTDATED);
  }

  plugin_version = get_config_value(fp, "plugin", "version");
  installed_status = get_config_value(fp, "plugin", "installed");

  if (installed_status && atoi(installed_status) != 0 &&
      plugin_version && plugin_version[0] &&
      (version = hplip_version(system)) != NULL)
  {
    if (!strcasecmp(plugin_version, version))
      status = HPLIP_PLUGIN_INSTALLED;
    else
      status = HPLIP_PLUGIN_OUTDATED;
    free(version);
  }

  if (plugin_version)
    free(plugin_version);
  if (installed_status)
    free(installed_status);
  fclose(fp);

  return (status);
}


//
// 'hplip_download_file() - Download a file from a given URL to a local
//                          temporary file
//

char*
hplip_download_file(pappl_system_t *system, const char *url)
{
  int           fd, status;
  char          tempfile[1024] = "";
  CURL *curl;
  FILE *fp = NULL;
  CURLcode ret;


  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Downloading %s", url);

  // Setup curl
  curl = curl_easy_init();
  if (curl)
  {
    // Create a temporary file
    fd = cupsTempFd(tempfile, sizeof(tempfile));
    if (fd < 0 || !tempfile[0])
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Unable to create temporary file");
      return (NULL);
    }

    // Download the file
    fp = fdopen(fd, "wb");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 10L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    ret = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);
    close(fd);
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to start curl");
    return (NULL);
  }

  // Check for errors
  if ((int)ret == 0)
    return(strdup(tempfile));
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Download status: %d", ret);
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to download file %s", url);
    unlink(tempfile);
  }

  return (NULL);
}


//
// 'hplip_run_command_line()' - Run a command line and log its screen output,
//                              both stdout and stderr. Return the exit code
//                              of the command.
//

int
hplip_run_command_line(pappl_system_t *system, const char *command)
{
  FILE *fp;
  char buf[1024];


  if ((fp = popen(command, "r")) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to start \"gpg\" utility fo plugin signature verification. Command: %s",
	     command);
    return (-1);
  }
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Executing command: %s", command);

  while (fgets(buf, sizeof(buf), fp) != NULL)
  {
    buf[strlen(buf) - 1] = '\0'; // Remove newline
    papplLog(system, PAPPL_LOGLEVEL_DEBUG, "  %s", buf);
  }

  return (pclose(fp));
}


//
// 'hplip_get_uncompress_dir() - Determine (and create) the directory
//                               where the plugin file should be
//                               uncompressed after download
//

char *
hplip_get_uncompress_dir(pappl_system_t *system, int create)
{
#ifdef HPLIP_PLUGIN_ALT_DIR

  (void)system;
  (void)create;

  return strdup(HPLIP_PLUGIN_ALT_DIR);

#else

  char buf[1024],
       *home;


  if ((home = getenv("HOME")) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "No HOME environment variable defined.");
    return (NULL);
  }

  snprintf(buf, sizeof(buf), "%s/.hplip", home);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Directory to uncompress the plugin in: %s", buf);

  if (create)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Creating directory for uncompressed plugin %s", buf);
    if (mkdir(buf, S_IRWXU) == -1)
    {
      if (errno != EEXIST)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "Error creating directory %s: %s", buf, strerror(errno));
	return (NULL);
      }
    }
  }

  return (strdup(buf));

#endif // HPLIP_PLUGIN_ALT_DIR
}


//
// 'hplip_remove_uncompress_dir() - Remove the uncompressed plugin
//                                  file when we do not need it any more
//

int
hplip_remove_uncompress_dir(pappl_system_t *system, const char *name)
{
  char *uncompress_dir;
  char buf[1024];
  DIR *d;
  struct dirent *entry;
  char *filename;
  int len;


  // Find the directory where the uncompressed plugin is located
  if ((uncompress_dir = hplip_get_uncompress_dir(system, 0)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not determine the directory with the uncompressed plugin file.");
    return (0);
  }

  // Open the directory with the files of the uncompressed plugin
  len = snprintf(buf, sizeof(buf), "%s/%s", uncompress_dir, name);
  if ((d = opendir(buf)) == NULL && errno != ENOENT)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not open the directory %s: %s", buf, strerror(errno));
    free(uncompress_dir);
    return (0);
  }

  // Remove the files of the plugin (only regular files and symlinks, no
  // sub-directories)
  if (d)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Removing uncompressed plugin directory %s", buf);
    while (entry = readdir(d))
    {
      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
	continue;
      snprintf(buf + len, sizeof(buf) - len, "/%s", entry->d_name);
      if (unlink(buf) != 0)
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "Could not remove file %s: %s", buf, strerror(errno));
    }
    closedir(d);

    // Remove the emptied directory
    buf[len] = '\0';
    if (rmdir(buf) != 0)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Could not remove directory %s: %s", buf, strerror(errno));
      free(uncompress_dir);
      return (0);
    }
  }

#ifndef HPLIP_PLUGIN_ALT_DIR

  // Try to remove the directory in which we had uncompressed the
  // plugin, but ignore errors (for example if it contains something
  // else, then we most probably did not create it in the first place)
  rmdir(uncompress_dir);
  if (errno == 0)
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Removed directory for uncompressed plugin %s", uncompress_dir);

#endif // !HPLIP_PLUGIN_ALT_DIR

  free(uncompress_dir);
  return (1);
}


//
// 'hplip_download_plugin()' - Download the plugin from the HP's official
//                             locations, validate, and uncompress it
//

char *
hplip_download_plugin(pappl_system_t *system)
{
  int i;
  char *plugin_conf = NULL,
       *plugin_file = NULL,
       *signature_file = NULL,
       *version = NULL,
       *url = NULL,
       *size_str = NULL,
       *checksum = NULL,
       *uncompress_dir = NULL,
       *ret = NULL;
  FILE *fp;
  size_t plugin_size;
  char buf[1024];
  struct stat st;
  int fd;
  int bytes;
  SHA_CTX ctx;
  unsigned char hash[SHA_DIGEST_LENGTH];
  int status;


  // Get plugin index file from HP
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Getting plugin index from HP ...");
  if ((plugin_conf = hplip_download_file(system, PLUGIN_CONF_URL)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to download plugin index");
    goto out;
  }

  // HPLIP version which we have installed, equals section name in the plugin
  // index
  if ((version = hplip_version(system)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to determine version of HPLIP");
    goto out;
  }

  // Open downloaded plugin index
  if ((fp = fopen(plugin_conf, "r")) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to open downloaded plugin index file");
    goto out;
  }

  // Read needed data items: URL, size, checksum
  if ((url = get_config_value(fp, version, "url")) == NULL || !url[0])
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not find plugin URL in index file. Is HPLIP %s already released?",
	     version);
    goto out;
  }

  if ((size_str = get_config_value(fp, version, "size")) == NULL ||
      !size_str[0] || (plugin_size = atoi(size_str)) <= 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not find plugin size in index file.");
    goto out;
  }

  if ((checksum = get_config_value(fp, version, "checksum")) == NULL ||
      !checksum[0])
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not find plugin checksum in index file.");
    goto out;
  }

  fclose(fp);

  // Download the plugin file
  buf[0] = '\0';
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Getting plugin file for HPLIP %s from official location ...",
	   version);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Trying URL: %s", url);
  if ((plugin_file = hplip_download_file(system, url)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Unable to download plugin file, trying backup server");
    snprintf(buf, sizeof(buf) - 5, "%s/hplip-%s-plugin.run",
	     PLUGIN_ALT_LOCATION, version);
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Trying URL: %s", buf);
    if ((plugin_file = hplip_download_file(system, buf)) == NULL)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to download plugin file.");
      goto out;
    }
  }

  // Download the plugin signature file
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Getting plugin signature file from the same location as the plugin file ...");
  if (buf[0])
    strcpy(buf + strlen(buf), ".asc");
  else
    snprintf(buf, sizeof(buf), "%s.asc", url);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Using URL: %s", buf);
  if ((signature_file = hplip_download_file(system, buf)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to download plugin signature file.");
    goto out;
  }

  // Determine size of the downloaded plugin
  stat(plugin_file, &st);
  if (st.st_size != plugin_size)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Downloaded plugin file is not of expected size. File has %ld bytes but expected are %ld bytes.",
	     st.st_size, plugin_size);
    goto out;
  }
  else
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Downloaded file size OK (%ld).", st.st_size);

  // Verify sha1sum of the plugin file against the checksum from the index file
  SHA1_Init(&ctx);
  fd = open(plugin_file, O_RDONLY);
  while ((bytes = read(fd, buf, sizeof(buf))) > 0)
    SHA1_Update(&ctx, buf, bytes);
  close(fd);
  SHA1_Final(hash, &ctx);
  for (i = 0; i < SHA_DIGEST_LENGTH; i ++)
    snprintf(buf + 2 * i, 3, "%.2x", hash[i]);
  buf[2 * SHA_DIGEST_LENGTH] = '\0';
  if (strcasecmp(buf, checksum))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Checksum of the plugin file (%s) does not match checksum of the plugin index (%s).",
	     buf, checksum);
    goto out;
  }
  else
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Downloaded file checksum OK (%s).", buf);

  // Check GPG signature

  // We do not load HP's public key here (which is needed ofr verification), as
  // HP's original command does not work.
  // Command as of HPLIP 3.21.8:
  //   gpg --homedir ~ --no-permission-warning --keyserver pgp.mit.edu --recv-keys 0x4ABA2F66DBD5A95894910E0673D770CDA59047B9
  // So the actual command depends on the source code in use and how it got
  // patched. For Debian/Ubuntu it is (key got added to the package):
  //   gpg --homedir ~ --no-permission-warning --import /usr/share/hplip/signing-key.asc
  // Please add a suitable command to the start-up script for this Printer Application

  snprintf(buf, sizeof(buf),
	   "gpg --homedir ~ --no-permission-warning --verify %s %s 2>&1",
	   signature_file, plugin_file);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Verifying plugin's signature.");

  if ((status = hplip_run_command_line(system, buf)) != 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Plugin signature verification failed.");
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "NOTE: If this failure is due to a missing public key, we do not load the");
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "public key here as this step can vary with the used HPLIP source code.");
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "The command used in the original source code of HPLIP (3.21.8)");
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "  gpg --homedir ~ --no-permission-warning --keyserver pgp.mit.edu --recv-keys 0x4ABA2F66DBD5A95894910E0673D770CDA59047B9");
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "does not work.");
    goto out;
  }

  // Get the directory where to uncompress the plugin
  if ((uncompress_dir = hplip_get_uncompress_dir(system, 1)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not find/create the directory for uncompressing the plugin.");
    goto out;
  }

  // Make sure we have the directory name "plugin_tmp" free for uncompressing
  // our plugin
  if (!hplip_remove_uncompress_dir(system, "plugin_tmp"))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Old uncompressed plugin file is in the way: %s/plugin_tmp",
	     uncompress_dir);
    goto out;
  }

  // Uncompress the plugin
  snprintf(buf, sizeof(buf),
	   "cd %s 2>&1 && mkdir plugin_tmp 2>&1 && cd plugin_tmp 2>&1 && sh %s --tar xf --no-same-owner 2>&1 && cd .. 2>&1 && chmod -R go+rX plugin_tmp 2>&1",
	   uncompress_dir, plugin_file);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Uncompressing the plugin file.");

  if ((status = hplip_run_command_line(system, buf)) != 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to uncompress the plugin");
    goto out;
  }

  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Downloaded, verified, and uncompressed the plugin successfully. Ready to install in %s/plugin_tmp",
	   uncompress_dir);

  // Output the uncompress location
  ret = uncompress_dir;

 out:

  // Clean up
  if (plugin_conf)
  {
    unlink(plugin_conf);
    free(plugin_conf);
  }
  if (signature_file)
  {
    unlink(signature_file);
    free(signature_file);
  }
  if (version)
    free(version);
  if (url)
    free(url);
  if (size_str)
    free(size_str);
  if (checksum)
    free(checksum);
  if (plugin_file)
  {
    unlink(plugin_file);
    free(plugin_file);
  }
  if (ret == NULL && uncompress_dir)
  {
    hplip_remove_uncompress_dir(system, "plugin_tmp");
    free(uncompress_dir);
  }

  return (ret);
}


#ifdef SNAP
//
// 'hplip_register_plugin() - Register the plugin installation or
//                            removal in the hplip,state file (in the
//                            Snap only).
//

int
hplip_register_plugin(pappl_system_t *system, const char *installed,
		      const char *eula, const char *version)
{
  int ret = 0;
  char buf[1024];
  char *filebuf = NULL;
  int size_needed;
  FILE *fp;


  // Register installation or removal of plugin in hplip.state
  // Open current file, if present
  snprintf(buf, sizeof(buf), "%s/%s", HPLIP_PLUGIN_STATE_DIR, "hplip.state");
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Registering plugin installation status in %s", buf);
  if ((fp = fopen(buf, "r")) == NULL && errno != ENOENT)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to open HPLIP plugin status file %s: %s",
	     buf, strerror(errno));
    goto out;
  }

  if (fp)
  {
    // Load complete file into a buffer (if we have a state file)
    fseek(fp, 0L, SEEK_END);
    size_needed = ftell(fp);
    filebuf = (char *)calloc(size_needed + 1, sizeof(char));
    rewind(fp);
    if (fread(filebuf, 1, size_needed, fp) != size_needed)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Unable to read HPLIP plugin status file %s: %s",
	       buf, strerror(errno));
      fclose(fp);
      goto out;
    }
    fclose(fp);
    filebuf[size_needed] = '\0';
  }

  // Modify the values in the buffer
  if (set_config_value(&filebuf, "plugin", "installed", installed) +
      set_config_value(&filebuf, "plugin", "eula", eula) +
      set_config_value(&filebuf, "plugin", "version", version) > 0)
  {
    // File has changed, rewrite it
    // Remove the old file
    if (unlink(buf) != 0 && errno != ENOENT)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Unable to delete old HPLIP plugin status file %s: %s",
	       buf, strerror(errno));
      free(filebuf);
      goto out;
    }

    // Write new file
    if ((fp = fopen(buf, "w")) == NULL)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Unable to open HPLIP plugin status file %s: %s",
	       buf, strerror(errno));
      free(filebuf);
      goto out;
    }
    if (fwrite(filebuf, 1, strlen(filebuf), fp) != strlen(filebuf))
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Unable to write to HPLIP plugin status file %s: %s",
	       buf, strerror(errno));
      free(filebuf);
      fclose(fp);
      goto out;
    }

    fclose(fp);
  }

  // Done
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Registered plugin installation status successfully.");
  ret = 1;

 out:

  return (ret);
}
#endif // SNAP


//
// 'hplip_install_plugin() - Install the downloaded plugin, after the
//                           license got accepted in the web
//                           interface, or right after uncompressing
//                           during an update. Use HP's Python script
//                           installPlugin.py contained in the plugin
//                           for a classic HPLIP installation and
//                           simply rename the plugin_tmp directory to
//                           plugin (deleting any old plugin/
//                           directory first) and synlink the dynamic
//                           link library (*.so) files of the system's
//                           architecture for the Snap. The Printer
//                           Application must run as root to install
//                           the plugin.
//

int
hplip_install_plugin(pappl_system_t *system, const char *plugin_dir)
{
  int ret = 0;

#ifdef SNAP

  int len;
  char buf1[1024], buf2[1024];
  DIR *d;
  struct dirent *entry;
  char *p, *version, *filebuf = NULL;
  int size_needed;
  FILE *fp;

  // Open the directory with the files of the uncompressed plugin
  len = snprintf(buf1, sizeof(buf1), "%s/plugin_tmp", plugin_dir);
  if ((d = opendir(buf1)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not open the directory %s: %s", buf1, strerror(errno));
    goto out;
  }

  // Go through all the files of the plugin, and for the dynamic link
  // libraries (*.so files) link the ones of our system's architecture
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Adding symlinks for dynamic link libraries of the %s architecture in %s", ARCH, buf1);
  buf1[len] = '/';
  len ++;
  while (entry = readdir(d))
  {
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
      continue;
    if ((p = strstr(entry->d_name, ARCH)) != NULL &&
	p > entry->d_name && *(p - 1) == '-' &&
	strncmp(p + strlen(ARCH), ".so", 3) == 0)
    {
      memmove(buf1 + len, entry->d_name, (p - entry->d_name) - 1);
      memmove(buf1 + len + (p - entry->d_name) - 1, p + strlen(ARCH),
	      strlen(p + strlen(ARCH)) + 1);
      if (symlink(entry->d_name, buf1) != 0)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "Could not create symboiic link %s to %s: %s",
		 buf1, entry->d_name, strerror(errno));
	goto out;
      }
    }
  }
  closedir(d);

  // Remove a previous version of the plugin
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Removing previous plugin version if present: %s/plugin",
	   plugin_dir);
  if (hplip_remove_uncompress_dir(system, "plugin") == 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to remove old plugin version %s/plugin", plugin_dir);
    goto out;
  }

  // Rename the plugin directory from plugin_tmp to plugin
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Renaming plugin directory to %s/plugin", plugin_dir);
  snprintf(buf1, sizeof(buf1), "%s/plugin_tmp", plugin_dir);
  snprintf(buf2, sizeof(buf2), "%s/plugin", plugin_dir);
  if (rename(buf1, buf2) != 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Could not rename directory %s to %s: %s",
	     buf1, buf2, strerror(errno));
    goto out;
  }

  // Get HPLIP (and now also plugin) version
  if ((version = hplip_version(system)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to determine version of HPLIP");
    goto out;
  }

  // Register installed plugin version in hplip.state
  if (!hplip_register_plugin(system, "1", "1", version))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to register HPLIP plugin installation status.");
    free(version);
    goto out;
  }

  free(version);

#else

  char buf[1024];

  // Run HP's script (included in the plugin) to install the plugin
  snprintf(buf, sizeof(buf),
	   "cd %s/plugin_tmp 2>&1 && python3 installPlugin.py 2>&1",
	   plugin_dir);
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Installing the plugin.");

  if (hplip_run_command_line(system, buf) != 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to install the plugin");
    goto out;
  }

#endif // SNAP

  // Done
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Installed the plugin successfully.");
  ret = 1;

 out:

  // Remove the uncompressed plugin file
  hplip_remove_uncompress_dir(system, "plugin_tmp");

  return (ret);
}


#ifdef SNAP
//
// 'hplip_remove_plugin() - Uninstall the installed plugin (in the
//                          Snap only), by removing its directory and
//                          unregistering its presence in the plugin
//                          state file.
//

int
hplip_remove_plugin(pappl_system_t *system, const char *plugin_dir)
{
  int ret = 0;
  char buf[1024];
  char *filebuf = NULL;
  int size_needed;
  FILE *fp;


  // Remove the plugin directory
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Removing plugin directory %s/plugin",
	   plugin_dir);
  if (hplip_remove_uncompress_dir(system, "plugin") == 0)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to remove plugin directory %s/plugin", plugin_dir);
    goto out;
  }

  // Register removal of plugin in hplip.state
  if (!hplip_register_plugin(system, "0", NULL, NULL))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR,
	     "Unable to register HPLIP plugin installation status.");
    goto out;
  }

  // Done
  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	   "Uninstalled the plugin successfully.");
  ret = 1;

 out:

  return (ret);
}
#endif // SNAP


//
// 'hplip_web_plugin()' - Web interface page for installing, updating,
//                        and removing HP's proprietary plugin.
//

void
hplip_web_plugin(
    pappl_client_t *client,		// I - Client
    void *data)                         // I - Global data
{
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  pappl_system_t      *system = prGetSystem(global_data); // System
  const char          *status = NULL;	// Status text, if any
  const char          *uri = NULL;      // Client URI
  pappl_version_t     version;
  hplip_plugin_status_t plugin_status;
  char                *plugin_dir = NULL;
  char                buf[2048];
  char                *licensetext = NULL;
  FILE                *fp;
  size_t              size_needed;


  if (!papplClientHTMLAuthorize(client))
    return;

  // Get status of installed plugin
  plugin_status = hplip_plugin_status(system);

  // Handle POSTs to add and delete PPD files...
  if (papplClientGetMethod(client) == HTTP_STATE_POST)
  {
    int			num_form = 0;	// Number of form variables
    cups_option_t	*form = NULL;	// Form variables
    const char		*action;	// Form action

    if ((num_form = papplClientGetForm(client, &form)) == 0)
    {
      status = "Invalid form data.";
    }
    else if (!papplClientIsValidForm(client, num_form, form))
    {
      status = "Invalid form submission.";
    }
    else if ((action = cupsGetOption("action", num_form, form)) == NULL)
    {
      status = "Missing action.";
    }
    else if (!strcmp(action, "install-plugin") ||
	     !strcmp(action, "install-plugin-yes") ||
	     !strcmp(action, "license-accepted"))
    {
      // Set status to trigger the "Are you sure?" page when we
      // re-install over an already installed and ip-to-date plugin
      status = "Installing plugin";
      if (strcmp(action, "license-accepted") &&
	  (plugin_status != HPLIP_PLUGIN_INSTALLED ||
	   !strcmp(action, "install-plugin-yes")))
      {
	// Download the plugin
	// Plugin installation only works if we are running as root
	if (!getuid())
        {
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "Downloading the proprietary plugin ...");
	  if ((plugin_dir = hplip_download_plugin(system)) == NULL)
	    papplLog(system, PAPPL_LOGLEVEL_ERROR,
		     "Unable to download plugin ...");
	  else
	    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		     "Plugin downloaded to %s", plugin_dir);
	}
	else
	  papplLog(system, PAPPL_LOGLEVEL_ERROR,
		   "Printer Application must run as root to download/install plugin.");
	if (plugin_dir)
	  // Succeeded, set status so that if no installation follows now
	  // we get onto the license page (if plugin_status ==
	  // HPLIP_PLUGIN_NOT_INSTALLED)
	  status = "Plugin downloaded.";
	else
	  // Failed, get back to plugin status page
	  status = "Plugin download failed.";
      }
      if (plugin_status == HPLIP_PLUGIN_OUTDATED ||
	  !strcmp(action, "install-plugin-yes") ||
	  !strcmp(action, "license-accepted"))
      {
	// Install the plugin
	// If failed, get back to plugin status page
	status = "Plugin installation failed.";
	// Plugin installation only works if we are running as root
	if (!getuid())
        {
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "Installing the proprietary plugin ...");
	  if (!plugin_dir)
	    plugin_dir = hplip_get_uncompress_dir(system, 0);
	  if (plugin_dir)
	  {
	    if (hplip_install_plugin(system, plugin_dir))
	    {
	      // Succeeded, update plugin status and get back to plugin
	      // status page
	      plugin_status = HPLIP_PLUGIN_INSTALLED;
	      status = "Plugin installed.";
	      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		       "Plugin installed.");
	    }
	    else
	      papplLog(system, PAPPL_LOGLEVEL_ERROR,
		       "Plugin installation failed.");
	  }
	  else
	    papplLog(system, PAPPL_LOGLEVEL_ERROR,
		     "Could not find/the directory with the downloaded plugin.");
	}
      }
    }
#if SNAP
    else if (!strcmp(action, "remove-plugin"))
    {
      // Only set status to trigger the "Are you sure?" page
      status = "Removing plugin.";
    }
    else if (!strcmp(action, "remove-plugin-yes"))
    {
      // Remove the plugin
      // If failed, get back to plugin status page
      status = "Plugin removal failed.";
      // Plugin installation only works if we are running as root
      if (!getuid())
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "Removing the proprietary plugin ...");
	if (!plugin_dir)
	  plugin_dir = hplip_get_uncompress_dir(system, 0);
	if (plugin_dir)
	{
	  if (hplip_remove_plugin(system, plugin_dir))
	  {
	    // Succeeded, update plugin status and get back to plugin
	    // status page
	    plugin_status = HPLIP_PLUGIN_NOT_INSTALLED;
	    status = "Plugin removed.";
	    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		     "Plugin removed.");
	  }
	  else
	    papplLog(system, PAPPL_LOGLEVEL_ERROR,
		     "Plugin removal failed.");
	}
	else
	  papplLog(system, PAPPL_LOGLEVEL_ERROR,
		   "Could not find/the directory with the installed plugin.");
      }
    }
#endif
    else if (!strcmp(action, "license-declined"))
    {
      // License declined
      // Remove downloaded and uncompressed plugin (plugin_tmp)
      if (!plugin_dir)
	plugin_dir = hplip_get_uncompress_dir(system, 0);
      if (plugin_dir)
      {
	if (hplip_remove_uncompress_dir(system, "plugin_tmp") == 0)
	{
	  papplLog(system, PAPPL_LOGLEVEL_ERROR,
		   "Unable to remove plugin directory %s/plugin_tmp",
		   plugin_dir);
	}
      }
      // Set status to get back onto plugin status page
      status = "License declined, plugin not installed.";
    }
    else if (!strcmp(action, "install-cancel"))
    {
      // "Are you sure?" on re-intall canceled
      // Only set status to get back onto plugin status page
      status = "Plugin not re-installed.";
    }
#if SNAP
    else if (!strcmp(action, "remove-cancel"))
    {
      // "Are you sure?" on remove canceled
      // Only set status to get back onto plugin status page
      status = "Plugin not removed.";
    }
#endif // SNAP
    else
      status = "Unknown action.";

    cupsFreeOptions(num_form, form);
  }

  // Find license file
  buf[0] = '\0';
  if (status && strcasestr(status, "downloaded") && plugin_dir)
    // Load license text from downloaded plugin
    snprintf(buf, sizeof(buf), "%s/plugin_tmp/license.txt", plugin_dir);
  else if (plugin_status != HPLIP_PLUGIN_NOT_INSTALLED)
  {
    // Load license text from installed plugin
#if SNAP
    if (!plugin_dir)
      plugin_dir = hplip_get_uncompress_dir(system, 0);
    snprintf(buf, sizeof(buf), "%s/plugin/license.txt", plugin_dir);
#else
    char *hplip_home = NULL;

    snprintf(buf, sizeof(buf), "%s/%s", HPLIP_CONF_DIR, "hplip.conf");
    if ((fp = fopen(buf, "r")) == NULL)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Unable to open HPLIP configuration file %s", buf);
      buf[0] = '\0';
    }
    else
    {
      hplip_home = get_config_value(fp, "dirs", "home");
      fclose(fp);
      if (hplip_home)
      {
	snprintf(buf, sizeof(buf), "%s/data/plugins/license.txt", hplip_home);
	free(hplip_home);
      }
      else
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "Unable to locate HPLIP data directory via the config file %s, cannot load license text",
		 buf);
	buf[0] = '\0';
      }
    }
#endif // SNAP
  }

  // Load license text 
  if (buf[0])
  {
    // Open license file
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Loading license text from %s", buf);
    if ((fp = fopen(buf, "r")) == NULL)
    {
      papplLog(system, PAPPL_LOGLEVEL_ERROR,
	       "Unable to open plugin license file %s: %s",
	       buf, strerror(errno));
    }
    else
    {
      // Load complete file into a buffer
      fseek(fp, 0L, SEEK_END);
      size_needed = ftell(fp);
      licensetext = (char *)calloc(size_needed + 1, sizeof(char));
      rewind(fp);
      if (fread(licensetext, 1, size_needed, fp) != size_needed)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "Unable to read plugin license file %s: %s",
		 buf, strerror(errno));
	free(licensetext);
	licensetext = strdup("Unable to load license file.");
	papplClientRespondRedirect(client, HTTP_STATUS_FOUND, "/plugin");
      }
      fclose(fp);
    }
  }

  // Output web interface page
  if (!papplClientRespond(client, HTTP_STATUS_OK, NULL, "text/html", 0, 0))
    goto clean_up;
  papplClientHTMLHeader(client, "Proprietary Plugin for HPLIP", 0);
  if (papplSystemGetVersions(system, 1, &version) > 0)
    papplClientHTMLPrintf(client,
                          "    <div class=\"header2\">\n"
                          "      <div class=\"row\">\n"
                          "        <div class=\"col-12 nav\">\n"
                          "          Version %s\n"
                          "        </div>\n"
                          "      </div>\n"
                          "    </div>\n", version.sversion);
  papplClientHTMLPuts(client, "    <div class=\"content\">\n");

  papplClientHTMLPrintf(client,
			"      <div class=\"row\">\n"
			"        <div class=\"col-12\">\n"
			"          <h1 class=\"title\">Proprietary Plugin for HPLIP</h1>\n");

  if (status)
    papplClientHTMLPrintf(client, "          <div class=\"banner\">%s</div>\n", status);

  if (status && strcasestr(status, "downloaded"))
  {
    // Show license text and buttons to accept and to reject installation
    // plugin_dir /plugin_tmp/license.txt

    // Display license text
    papplClientHTMLPuts(client,
			"        <p>You are about to install the proprietary plugin from HP. You have to agree with the following license to use it:\n");
    papplClientHTMLPrintf(client,
			  "        <div class=\"log\"><pre>%s</pre></div>\n",
			  licensetext);

    uri = papplClientGetURI(client);

    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n");

    papplClientHTMLPuts(client, "          <tr><td><button type=\"submit\" name=\"action\" value=\"license-accepted\">Agree</button>");
    papplClientHTMLPuts(client, "&nbsp;<button type=\"submit\" name=\"action\" value=\"license-declined\">Decline</button>");
    papplClientHTMLPuts(client,
			"</td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </form>\n");
  }
  else if (status && strcasestr(status, "installing"))
  {
    // Ask the user whether he is sure to re-install the plugin
    papplClientHTMLPuts(client,
			"        <p>You are about to re-install the already installed plugin from HP. This is only needed if you are facing problems with the plugin.</p>\n");
    papplClientHTMLPuts(client,
			"        <p>Are you sure?</p>\n");

    uri = papplClientGetURI(client);

    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n");

    papplClientHTMLPuts(client, "          <tr><td><button type=\"submit\" name=\"action\" value=\"install-plugin-yes\">Re-Install Plugin</button>");
    papplClientHTMLPuts(client, "&nbsp;<button type=\"submit\" name=\"action\" value=\"install-cancel\">Cancel</button>");
    papplClientHTMLPuts(client,
			"</td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </form>\n");
  }
#if SNAP
  else if (status && strcasestr(status, "removing"))
  {
    // Ask the user whether he is sure to remove the plugin
    papplClientHTMLPuts(client,
			"        <p>You are about to remove the installed plugin from HP. This is only needed if you are facing problems with the plugin, and if you have devices which need the plugin, they will stop working.</p>\n");
    papplClientHTMLPuts(client,
			"        <p>Are you sure?</p>\n");

    uri = papplClientGetURI(client);

    papplClientHTMLStartForm(client, uri, false);
    papplClientHTMLPuts(client,
			"          <table class=\"form\">\n"
			"            <tbody>\n");

    papplClientHTMLPuts(client, "          <tr><td><button type=\"submit\" name=\"action\" value=\"remove-plugin-yes\">Remove Plugin</button>");
    papplClientHTMLPuts(client, "&nbsp;<button type=\"submit\" name=\"action\" value=\"remove-cancel\">Cancel</button>");
    papplClientHTMLPuts(client,
			"</td></tr>\n"
			"            </tbody>\n"
			"          </table>\n"
			"        </form>\n");
  }
#endif // SNAP
  else
  {
    // Show plugin status and buttons to install, re-install, uninstall plugin

    papplClientHTMLPuts(client,
			"        <p>Some devices cannot be supported by HP's free-(open-source) software driver suite <a href=\"https://developers.hp.com/hp-linux-imaging-and-printing\">HPLIP</a> (HP Linux Imaging and Printing) but need driver extensions or firmware files which are proprietary software (binary-only, no source code published) which cannot be distributed in this Printer Application or in Linux distributions. Therefore all these pieces of proprietary software are packaged together by HP to form a single plugin package ");
    if (getuid())
      papplClientHTMLPuts(client,
			  "which can be installed with HPLIP.</p>\n");
    else
      papplClientHTMLPuts(client,
			  "which can be installed here.</p>\n");
    papplClientHTMLPuts(client,
		      "        <p>In most cases, for ~95% of the supported printers you do not need to install it. Only if the printer model you have selected on the \"Add Printer\" page is marked with \"requires proprietary plugin\" or you find this remark in the print queue entry once you have set up your printer, you need to install this plugin.</p>\n");
#if SNAP
    papplClientHTMLPuts(client,
			"        <p>With each new release of HPLIP by HP also a new version of the plugin is issued. You do not need to do any manual updates, this Snap gets auto-updated and if the update contains a new version of HPLIP, also the plugin (if it is installed) gets updated.</p>\n");
#else
    papplClientHTMLPuts(client,
			"        <p>With each new release of HPLIP by HP also a new version of the plugin is issued. You do not need to do any manual updates, this Printer Application automatically updates the plugin (if it is installed) when a new version of HPLIP gets installed on the system.</p>\n");
#endif // SNAP
    papplClientHTMLPuts(client,
			"        <h3>Plugin installation status</h3>\n");
    papplClientHTMLPrintf(client,
			  "        <p><blockquote><b style=\"font-size:200%%;\">%s</b></blockquote></p>\n",
			  plugin_status == HPLIP_PLUGIN_NOT_INSTALLED ?
			  "Plugin NOT installed" :
			  (plugin_status == HPLIP_PLUGIN_INSTALLED ?
			   "Plugin installed and up-to-date" :
			   (plugin_status == HPLIP_PLUGIN_OUTDATED ?
			    "Plugin out-of-date" :
			    "Plugin status unknown")));

    if (getuid())
    {
      if (plugin_status != HPLIP_PLUGIN_INSTALLED)
	papplClientHTMLPuts(client,
			    "        <p>To install or update the proprietary plugin please use the \"<font face=\"Courier\">hp-plugin</font>\" utility of HPLIP.</p>\n");
    }
    else
    {
      uri = papplClientGetURI(client);

      papplClientHTMLStartForm(client, uri, false);
      papplClientHTMLPuts(client,
			  "          <table class=\"form\">\n"
			  "            <tbody>\n");

      papplClientHTMLPrintf(client, "          <tr><td><button type=\"submit\" name=\"action\" value=\"install-plugin\">%s</button>",
			    plugin_status == HPLIP_PLUGIN_NOT_INSTALLED ?
			    "Install Plugin" :
			    (plugin_status == HPLIP_PLUGIN_INSTALLED ?
			     "Re-install Plugin" :
			     (plugin_status == HPLIP_PLUGIN_OUTDATED ?
			      "Update Plugin" :
			      "Install/Update Plugin")));
#if SNAP
      if (plugin_status != HPLIP_PLUGIN_NOT_INSTALLED)
	papplClientHTMLPuts(client, "&nbsp;<button type=\"submit\" name=\"action\" value=\"remove-plugin\">Remove Plugin</button>");
#endif // SNAP
      papplClientHTMLPuts(client,
			  "</td></tr>\n"
			  "            </tbody>\n"
			  "          </table>\n"
			  "        </form>\n");
    }

    if (plugin_status != HPLIP_PLUGIN_NOT_INSTALLED && licensetext)
    {
      papplClientHTMLPuts(client,
			  "        <h3>License</h3>\n");
      papplClientHTMLPuts(client,
			  "        <p>The proprietary plugin of HPLIP is released under the following user license:</p>\n");
      papplClientHTMLPrintf(client,
			    "        <div class=\"log\"><pre>%s</pre></div>\n",
			    licensetext);
    }
  }

  papplClientHTMLPuts(client,
                      "      </div>\n"
                      "    </div>\n");
  papplClientHTMLFooter(client);

 clean_up:
  // Clean up
  if (plugin_dir)
    free(plugin_dir);
  if (licensetext)
    free(licensetext);
}


//
// 'hplip_plugin_support()' - Callback function for the system
//                            setup. It updates an already installed
//                            plugin, if HPLIP got update to a new
//                            upstream version, it adds a button to
//                            the system part of the main page of the
//                            web interface, to open the page to
//                            initially install the plugin, and it
//                            adds this page to the web interface.
//

void
hplip_plugin_support(void *data)
{
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  pappl_system_t   *system = prGetSystem(global_data);
  hplip_plugin_status_t plugin_status;
  char             *plugin_dir;


  // Get status of installed plugin
  plugin_status = hplip_plugin_status(system);

  if (plugin_status == HPLIP_PLUGIN_NOT_INSTALLED)
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Proprietary plugin is not installed.");
  else if (plugin_status == HPLIP_PLUGIN_INSTALLED)
    papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	     "Proprietary plugin is installed and up-to-date.");
  else if (plugin_status == HPLIP_PLUGIN_OUTDATED)
  {
    // We need to update the plugin

    // Plugin installation only works if we are running as root
    if (!getuid())
    {
      papplLog(system, PAPPL_LOGLEVEL_DEBUG,
	       "Updating an already installed proprietary plugin ...");
      if ((plugin_dir = hplip_download_plugin(system)) == NULL)
      {
	papplLog(system, PAPPL_LOGLEVEL_ERROR,
		 "Unable to download plugin ...");
      }
      else
      {
	papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		 "Plugin downloaded to %s", plugin_dir);

	if (hplip_install_plugin(system, plugin_dir))
	  papplLog(system, PAPPL_LOGLEVEL_DEBUG,
		   "Plugin installed.");
	else
	  papplLog(system, PAPPL_LOGLEVEL_ERROR,
		   "Plugin installation failed.");

	free(plugin_dir);
      }
    }
  }

  // Add web interface page to manage the plugin
  papplSystemAddResourceCallback(system, "/plugin", "text/html",
				 (pappl_resource_cb_t)hplip_web_plugin,
				 global_data);
  papplSystemAddLink(system,
		     getuid() ? "Proprietary Plugin Status" :
		     "Install Proprietary Plugin",
		     "/plugin",
		     PAPPL_LOPTIONS_OTHER | PAPPL_LOPTIONS_HTTPS_REQUIRED);
}


//
// 'hplip_printer_extra_web_if()' - Add button for plugin web
//                                  interface page to print queue
//                                  entries which need the plugin.
//                                  Also call original function for
//                                  adding the "Device Settings",
//                                  page, needed especially for the
//                                  PostScript printers.
//

void
hplip_printer_extra_web_if(pappl_printer_t *printer, // I - Printer
			   void *data)               // I - Global data
{
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  pappl_system_t   *system = prGetSystem(global_data);
  pappl_pr_driver_data_t driver_data;


  // "Device Settings" page for PPDs with "Installable Options" group or
  // PostScript query code for printer settings
  prSetupDeviceSettingsPage(printer, data);

  papplPrinterGetDriverData(printer, &driver_data);
  if (strcasestr(driver_data.make_and_model, "proprietary plugin"))
  {
    // Printer needs the plugin, add a "Plugin" button to get to the
    // plugin management web interface page to the printer's entry
    papplPrinterAddLink(printer, "Plugin", "/plugin",
			PAPPL_LOPTIONS_NAVIGATION | PAPPL_LOPTIONS_STATUS);
  }
}


//
// 'main()' - Main entry for the hplip-printer-app.
//

int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  cups_array_t *spooling_conversions,
               *stream_formats,
               *driver_selection_regex_list;

  // Array of spooling conversions, most desirables first
  //
  // Here we prefer not converting into another format
  // Keeping vector formats (like PS -> PDF) is usually more desirable
  // but as many printers have buggy PS interpreters we prefer converting
  // PDF to Raster and not to PS
  spooling_conversions = cupsArrayNew(NULL, NULL);
  cupsArrayAdd(spooling_conversions, (void *)&PR_CONVERT_PDF_TO_PS);
  cupsArrayAdd(spooling_conversions, (void *)&PR_CONVERT_PDF_TO_RASTER);
  cupsArrayAdd(spooling_conversions, (void *)&PR_CONVERT_PS_TO_PS);
  cupsArrayAdd(spooling_conversions, (void *)&PR_CONVERT_PS_TO_RASTER);

  // Array of stream formats, most desirables first
  //
  // PDF comes last because it is generally not streamable.
  // PostScript comes second as it is Ghostscript's streamable
  // input format.
  stream_formats = cupsArrayNew(NULL, NULL);
  cupsArrayAdd(stream_formats, (void *)&PR_STREAM_CUPS_RASTER);
  cupsArrayAdd(stream_formats, (void *)&PR_STREAM_POSTSCRIPT);

  // Configuration record of the Printer Application
  pr_printer_app_config_t printer_app_config =
  {
    SYSTEM_NAME,              // Display name for Printer Application
    SYSTEM_PACKAGE_NAME,      // Package/executable name
    SYSTEM_VERSION_STR,       // Version as a string
    {
      SYSTEM_VERSION_ARR_0,   // Version 1st number
      SYSTEM_VERSION_ARR_1,   //         2nd
      SYSTEM_VERSION_ARR_2,   //         3rd
      SYSTEM_VERSION_ARR_3    //         4th
    },
    SYSTEM_WEB_IF_FOOTER,     // Footer for web interface (in HTML)
    // pappl-retrofit special features to be used
    PR_COPTIONS_QUERY_PS_DEFAULTS |
    PR_COPTIONS_NO_GENERIC_DRIVER |
#if !SNAP
    PR_COPTIONS_USE_ONLY_MATCHING_NICKNAMES |
#endif // !SNAP
    PR_COPTIONS_NO_PAPPL_BACKENDS |
    PR_COPTIONS_CUPS_BACKENDS,
    prAutoAdd,               // Auto-add (driver assignment) callback
    NULL,                     // Printer identify callback (HPLIP backend
                              // does not support this)
    prTestPage,              // Test page print callback
    hplip_plugin_support,     // Update installed plugin during system setup
                              // and add web interface button and page for
                              // plugin download
    hplip_printer_extra_web_if, // Set up "Device Settings" printer web
                              // interface page and also add a link to
                              // the plugin web interface page to entries
                              // of printers which need the plugin.
    spooling_conversions,     // Array of data format conversion rules for
                              // printing in spooling mode
    stream_formats,           // Arrray for stream formats to be generated
                              // when printing in streaming mode
    "",                       // CUPS backends to be ignored
    "hp,HP,snmp,dnssd,usb",   // CUPS backends to be used exclusively
                              // If empty all but the ignored backends are used
    TESTPAGE,                 // Test page (printable file), used by the
                              // standard test print callback prTestPage()
    ", +hpcups +[0-9]+\\.[0-9]+\\.[0-9]+[, ]*(.*)$|(\\W*[Pp]ost[Ss]cript).*$",
                              // Regular expression to separate the
                              // extra information after make/model in
                              // the PPD's *NickName. Also extracts a
                              // contained driver name (by using
                              // parentheses)
    NULL
                              // Regular expression for the driver
                              // auto-selection to prioritize a driver
                              // when there is more than one for a
                              // given printer. If a regular
                              // expression matches on the driver
                              // name, the driver gets priority. If
                              // there is more than one matching
                              // driver, the driver name on which the
                              // earlier regular expression in the
                              // list matches, gets the priority.
  };

  return (prRetroFitPrinterApp(&printer_app_config, argc, argv));
}
