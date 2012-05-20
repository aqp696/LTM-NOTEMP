#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char **argv) {
    int niu, fd, proto;
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof (int))];

    fd = atoi(argv[1]);
    proto = atoi(argv[2]);

    if ((proto < 140) || (proto > 252)) {
        printf("El protocolo de red nº%d no es valido.\n", proto);
        return -1;
    }
    niu = socket(AF_INET, SOCK_RAW, proto);

    if (niu < 0) {
        fprintf(stderr, "%d error al abrir socket RAW\n", errno);
        return -1;
    }

    if (fcntl(niu, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "%d error i_red fcntl\n", errno);
        return -2;
    }

    if (fcntl(niu, F_SETFD, fcntl(niu, F_GETFD) & !FD_CLOEXEC) < 0) {
        fprintf(stderr, "%d error i_red fcntl_2\n", errno);
        return -3;
    }

    /* Transmit it via fd */
    msg.msg_control = buf;
    msg.msg_controllen = sizeof (buf);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof (int) * 1);

    memcpy(CMSG_DATA(cmsg), &niu, sizeof (int));
    msg.msg_controllen = cmsg->cmsg_len;

    int snd = -1;
    if ((snd = sendmsg(fd, &msg, 0)) < 0) {
        perror("sendmsg");
        return -4;
    }

    return 0;
}
