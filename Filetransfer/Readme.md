# Description
##Adaptive Network File Transfer System
- Implemented a file transfer application with C
- Implemented estimation of RTT 
- Provided abstraction layers by creating packet sending function that hides all the logic that deals with network reordering, network duplication, etc.
- Possible to transmit files of any sizes and types 

# design decision
If it’s a large file, you do not want to read the entire file into your virtual memory area, i.e. heap, at once. It is otherwise a very inefficient way to use memory. 

You don’t want transfer(FILE *) to call exit(1) immediately just because something went wrong. Imagine you opened a file transfer app, you’d want the flexibility for it to possibly keep running after your typo and let you retry without re-opening the app. 

Client Program
Fragments the files
make sure each packet is smaller than the maximum size of the receiving buffer on the other side
Don’t read the entire file into the memory at once
Stop-and-wait for acknowledgements
It probably means no timeout is needed

Server Program
Receive the first packet and create a new file if you can extract a 1 from the packet
Increment the “expected_fragment” and wait for the next one. 
Repeat the above step if received == expected
If received < expected, send the ACK again and don’t increment the expected_fragment
Close the file if the received_fragment == total_frag


# Haven't Done
- like selective-repeat (SR) reliable transferring