#/bin/bash

gcc -lcurl -lcjson -lpthread pb-cli.c -o pb-cli
sudo mv pb-cli /usr/bin/pb-cli