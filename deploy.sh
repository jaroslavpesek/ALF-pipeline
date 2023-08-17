#!/bin/bash

APP_NAME=alf
SERVER=sklepmistr
ROOT_PATH=$(pwd)

echo "Deploying $APP_NAME to $SERVER."
echo "Running Ansible playbook to prepare server..."

ansible-playbook -i $ROOT_PATH/ansible/inventory -l $SERVER -e "root_path=$ROOT_PATH app_name=$APP_NAME server_name=$SERVER.liberouter.org" $ROOT_PATH/ansible/playbook.yaml

echo "Done ansible. Waiting 10 seconds"
sleep 10
echo
echo "API status response:"
echo $(curl -s http://$SERVER.liberouter.org/api/)
echo
echo "Checking ${APP_NAME}ctl status:"
echo $(ssh $SERVER.liberouter.org "${APP_NAME}ctl status")
echo

