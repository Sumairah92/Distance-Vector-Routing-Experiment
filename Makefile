all:dvr
dvr:dvr.c
	gcc dvr.c -o dvr
clean:
	rm -rf *o dvr
