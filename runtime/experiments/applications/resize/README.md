# Resize

Resizes Images

It appears that the initial request cuts off the bottom of the image. Thereafter, it seems that the runtime crashes out due to a socket error.

```
write: Bad file descriptor
C: 07, T: 0x7f20eed26700, F: current_sandbox_main>
        Unable to build and send client response

C: 07, T: 0x7f20eed26700, F: client_socket_send>
        Error sending to client: Bad file descriptor
C: 07, T: 0x7f20eed26700, F: sandbox_close_http> PANIC!
        Bad file descriptor
find: 'result_13192.jpg': No such file or directory
```
