
export SCRIPT_HOME=/local
docker-machine rm -f debug
#docker-machine rm -f v1 v2 v3
. user1rc
nova keypair-add --pub-key $SCRIPT_HOME/data-population/key1.pub key1
imageid=`glance image-list | grep docker | awk -F \| '{print $2}' | tr -d \ `
nova container-update-policy --container-id 2 --template-id=tapcon --template-params "{\"image\": \"$imageid\"}"
#for n in 1 2 3; do
#bash $SCRIPT_HOME/data-population/launch.sh v$n 192.1.$n.0/24 2
#done
bash $SCRIPT_HOME/data-population/launch.sh debug 192.1.5.0/24 2
