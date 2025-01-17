# ubuntu22.04 

## 1. Ubuntu 22.04 上安装 `git-lfs`

### 问题：

```cmd
git: 'lfs' is not a git command. See 'git --help'.

The most similar command is
        log
```

出现该问题为 lfs 未安装

### 安装sudo工具

```cmd
apt-get install sudo  
```

### 解决方法：

安装lfs

方式一：

```cmd
sudo apt-get update
sudo apt-get -y install git-lfs
```

方式二：

使用 apt 安装 git-lfs

```cmd
# 1. apt 使用以下命令更新 apt 数据库。
sudo apt update
sudo apt -y install git-lfs
```

方式三：

使用 aptitude 安装 git-lfs

```cmd
sudo aptitude update
sudo aptitude -y install git-lfs
```

## 2. Ubuntu 上卸载 git-lfs 22.04

 1. 要仅卸载软件包

    ```cmd
    sudo apt-get remove git-lfs
    ```

 2. 卸载 git-lfs 及其依赖项

    ```cmd
    sudo apt-get -y autoremove git-lfs
    ```

 3. 删除 git-lfs 配置和数据 

    ```cmd
    sudo apt-get -y purge git-lfs
    ```

 4. 删除 git-lfs 配置、数据及其所有依赖项

    ```cmd
    sudo apt-get -y autoremove --purge git-lfs
    ```


## 3. 安装sudo





### 4. 服务器端口映射

```cmd
ssh -p 39855 -L 8501:192.168.235.123:8501 root@ssh.intern-ai.org.cn -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null


ssh -p 39855 -L 172.16.4.114:8500:192.168.225.224:8500 root@ssh.intern-ai.org.cn -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null

ssh -p 46916 -L 172.16.4.114:8500:172.17.0.9:8500 root@connect.cqa1.seetacloud.com
```

-- -p 远程服务器端口

-L 远程端口:远程ip:本地端口
