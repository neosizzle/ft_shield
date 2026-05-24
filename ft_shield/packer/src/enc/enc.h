#ifndef __ENC__H__
#define __ENC__H__

#define  KEY_SIZE  256

typedef struct		s_key
{
	char			*buffer;
	size_t			size;
}					t_key;


t_key *generate_key(int size);
void free_key(t_key *key);
char *xor_encrypt(char *input, size_t input_len, t_key *key);

#endif  //!__ENC__H__