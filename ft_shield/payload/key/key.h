#ifndef __KEY__H__
#define __KEY__H__
#define	PASS_SIZE 33
#define PASS_FILE_DEST "/tmp/ft_shield_pass" 

int generate_key(char *in);
int send_key(char *key);

#endif  //!__KEY__H__