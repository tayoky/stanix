#ifndef KERNEL_INI_H
#define KERNEL_INI_H

void read_main_conf_file(void);

/** @brief get the value of an key in a .ini conf file
 *  @param ini_file an pointer to the string (conf file)
 *  @param section the section name
 *  @param key the key name to search
 *  @return an string the caller must free
 */
char *ini_get_value(const char*ini_file,const char *section,const char *key);

#endif
