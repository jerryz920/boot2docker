cd linux
bash rebuild.sh
cd ..
docker build -t boot2docker .
docker run boot2docker > boot2docker.iso

id=`glance image-list | grep docker | awk -F\| '{print $2}' | tr -d \ `
glance image-delete $id
glance image-create --file boot2docker.iso --container-format bare --disk-format iso --name docker

