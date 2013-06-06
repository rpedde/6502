/*
 * Copyright (C) 2013 Ron Pedde <ron@pedde.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * simple tool to convert bin files to mif files for the purpose
 * of initializing vhdl block memory
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    uint8_t buffer[BUF_SIZE];
    char out_buffer[80];

    ssize_t bytes_read;
    int offset;

    int fd_in, fd_out;

    if(argc != 3) {
        fprintf(stderr, "Input and output file name must be specified\n");
        exit(EXIT_FAILURE);
    }

    fd_in = open(argv[1], O_RDONLY);

    if(fd_in < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    fd_out = open(argv[2], O_WRONLY | O_CREAT, 0666);

    if(fd_out < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    while((bytes_read = read(fd_in, &buffer, BUF_SIZE)) > 0) {
        offset = 0;
        while(offset < bytes_read) {
            sprintf(out_buffer, "%02x\n", buffer[offset]);
            offset++;
            write(fd_out, out_buffer, strlen(out_buffer));
        }
    }

    if(bytes_read == -1) {
        perror("read");
    }

    close(fd_in);
    close(fd_out);

    exit(EXIT_SUCCESS);
}
