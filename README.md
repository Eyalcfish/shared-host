# shared-host
IPC library for transferring information fast and with ease between process.

plans:

each one way communication has a circular buffer
the server writes to x and then x +1 and then x+2, the client reads x and then x+1 and then x+2 and if it ever reaches a empty slot, it waits (this design can advance in message sizes)