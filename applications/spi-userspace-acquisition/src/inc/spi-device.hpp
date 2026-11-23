#ifndef SPI_DEVICE_HPP
#define SPI_DEVICE_HPP

#define MAX_SPI_BUFSIZE 8192

extern char TXBuf[];
extern char RXBuf[];

int spiOpen(unsigned spiChan, unsigned spiBaud, unsigned spiFlags);

int spiClose(int fd);

int spiRead(int fd, unsigned speed, char *buf, unsigned count);

int spiWrite(int fd, unsigned speed, char *buf, unsigned count);

int spiXfer(int fd, unsigned speed, char *txBuf, char *rxBuf, unsigned count);

#endif
