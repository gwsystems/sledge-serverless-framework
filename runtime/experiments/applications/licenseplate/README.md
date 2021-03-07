# License Plate Detection

Draws a magenta bounding box around license plates located in an image.

The first request seems to succeed, but subsequent requests crash out due to a socket error.

```
write: Bad file descriptor
C: 05, T: 0x7f3412232700, F: current_sandbox_main>
        Unable to build and send client response

C: 05, T: 0x7f3412232700, F: client_socket_send>
        Error sending to client: Bad file descriptor
C: 05, T: 0x7f3412232700, F: sandbox_close_http> PANIC!
        Bad file descriptor
find: ‘result_23619.jpg’: No such file or directory
```
