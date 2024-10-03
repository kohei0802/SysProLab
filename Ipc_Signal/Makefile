# A Minimalistic Makefile
.PHONY: all

# rename with your uid, make sure no space after your uid
UID = [UID]

# Default target
all: receiver main

receiver:
	gcc -o receiver ReceiverProcess/receiverProcess.c

main:
	gcc -o main MainProcess/mainProcess.c

# Clean target to remove the downloaded file
clean:
	rm -f run main receiver

# clean_bin:
# 	rm -f model.bin tokenizer.bin