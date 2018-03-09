bash pack.sh
cd linux
bash rebuild.sh
cd ..
docker build -t boot2docker .
docker run boot2docker > boot2docker.iso

sleep 5
source adminrc
# sometimes it bugs out... Just give it some time
openstack user list -v
openstack user list -v
openstack user list -v

source user1rc
id=`glance image-list | grep docker | awk -F\| '{print $2}' | tr -d \ `
glance image-delete $id
echo glance image-create --file boot2docker.iso --container-format bare --disk-format iso --name docker
source user1rc
glance image-create --file boot2docker.iso --container-format bare --disk-format iso --name docker
if [ $? -ne 0 ]; then
id=`glance image-list | grep docker | awk -F\| '{print $2}' | tr -d \ `
glance image-delete $id
glance image-create --file boot2docker.iso --container-format bare --disk-format iso --name docker
fi

