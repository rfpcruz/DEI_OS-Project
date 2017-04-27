#!/bin/bash
# @rCruz


#Para correr fazer:
# bash consola.sh
#

clear;

echo " ******* Script para ciclo infinito de pedidos *********"
echo "            -- faz um pedido a cada  seg --            "
echo ""
echo "A usar o porto: $1"
echo ""


while :
do
    wget http://localhost:$1
    sleep 1
    wget http://localhost:$1/dijkstra.html
    sleep 1
    wget http://localhost:$1/rouxinol.html
    sleep 1
    wget http://localhost:$1/dei.html
    sleep 1
    wget http://localhost:$1/c.gz
    sleep 1
    wget http://localhost:$1/z.gz
    sleep 1
    wget http://localhost:$1
    sleep 1
    wget http://localhost:$1/dijkstra.html
    sleep 1
    wget http://localhost:$1/rouxinol.html
    sleep 1
    wget http://localhost:$1/dei.html
    sleep 1
    wget http://localhost:$1/a.gz
    sleep 1
    wget http://localhost:$1/b.gz
    sleep 1    
    wget http://localhost:$1/c.gz    
    sleep 1
    wget http://localhost:$1/c.gz    
    sleep 1
    wget http://localhost:$1/dijkstra.html
    sleep 1
    wget http://localhost:$1/rouxinol.html
    sleep 1
    wget http://localhost:$1/dei.html
    sleep 1
    wget http://localhost:$1/z.gz
    sleep 1
    wget http://localhost:$1/a.gz
    sleep 1
    wget http://localhost:$1/b.gz
done
