all: cpu-load

cpu-load: cpu-load.c
	$(CC) -Wall -Wextra $< -o $@

clean:
	rm -f cpu-load

install: cpu-load
	install -d ${DESTDIR}/usr/bin
	install cpu-load ${DESTDIR}/usr/bin

uninstall:
	rm ${DESTDIR}/usr/bin/cpu-load
