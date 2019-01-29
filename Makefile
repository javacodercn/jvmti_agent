all:libagent.so client
libagent.so:agent.o  log.o network.o
	gcc -o libagent.so -shared agent.o log.o network.o -fPIC
client:client.o  log.o
	gcc -o client client.o log.o
agent.o:
	gcc -c agent.c -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -o agent.o -fPIC
log.o:
	gcc -c log.c -o log.o -fPIC
network.o:network.c
	gcc -c network.c -o network.o -fPIC
client.o:client.c
	gcc -c client.c -o client.o

.PHONY:clean
clean:
	rm -rf *.o libagent.so client
