all: homeauto
	
homeauto: homeauto.o qshttpd.o
	gcc -o homeauto homeauto.o qshttpd.o -lbcm2835 -lcurl -lm
	mv homeauto ../
	cp homeauto.conf ../
	cp qshttpd.conf ../
		
homeauto.o: homeauto.c
	gcc -c homeauto.c
	
qshttpd.o: qshttpd.c
	gcc -c qshttpd.c

clean:
	rm *.o
