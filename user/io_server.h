#pragma once

void io_server_task();

/**
 * the tid refers to the server that handles the appropriate channel.
 * returns the next un-returned character. The first argument is the task id
 * of the appropriate I/O server. How communication errors are handled is implementation-dependent.
 * Getc() is actually a wrapper for a send to the appropriate server.
 *
 * Return Value
 * >=0	new character from the given UART.
 * -1	tid is not a valid uart server task.
 */
int Getc(int tid);

/**
 * the tid refers to the server that handles the appropriate channel.
 * queues the given character for transmission by the given UART. On return the only guarantee is
 * that the character has been queued. Whether it has been transmitted or received is not
 * guaranteed. How communication errors are handled is implementation-dependent. Putc() is actually
 * a wrapper for a send to the appropriate server.
 *
 * Return Value
 * 0	success.
 * -1	tid is not a valid uart server task.
 */
int Putc(int tid, unsigned char ch);

/**
 * the tid refers to the server that handles the appropriate channel.
 * queues the given array of characters for transmission by the given UART. On return the only
 * guarantee is that the character has been queued. Whether it has been transmitted or received is
 * not guaranteed. How communication errors are handled is implementation-dependent. Putl() is
 * actually a wrapper for a send to the appropriate server.
 *
 * Return Value
 * 0	success.
 * -1	tid is not a valid uart server task.
 */
int Putl(int tid, unsigned char *data, unsigned int len);
