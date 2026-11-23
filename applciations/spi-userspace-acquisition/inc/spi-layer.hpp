#ifndef SPI_LAYER_HPP
#define SPI_LAYER_HPP

#define MAX_SPI_BUFSIZ 8192

extern char RXBuf[MAX_SPI_BUFSIZ];
extern char TXBuf[MAX_SPI_BUFSIZ];
extern int speed;

double time_time(void);
int spiOpen(unsigned spiChan, unsigned spiBaud, unsigned spiFlags);
int spiClose(int fd);
int spiRead(int fd, unsigned speed, char *buf, unsigned count);
int spiWrite(int fd, unsigned speed, char *buf, unsigned count);
int spiXfer(int fd, unsigned speed, char *txBuf, char *rxBuf, unsigned count);

#endif // SPI_LAYER_HPP
