WebERA
======

### Install instructions:

1. Install the extended version of EventRacer from https://github.com/sema/EventRacer.
  
  Assuming that the prerequisites have been installed in advance:
  ```
  git clone https://github.com/sema/EventRacer ~/R4-EventRacer
  cd ~/R4-EventRacer
  ./build.sh
  ```

2. Clone the R4 repository:
  ```
  git clone https://github.com/eth-srl/R4 ~/R4
  ```

3. Prepare the environment:
  ```
  export WEBERA_DIR=~/R4
  export EVENTRACER_DIR=~/R4-EventRacer
  export LD_LIBRARY_PATH=~/R4/WebKitBuild/Release/lib
  export QTDIR=/usr/share/qt4
  export PATH=$QTDIR/bin:$PATH
  ```

4. Build WebKit:
  ```
  cd ~/R4
  ./build.sh
  ```

### Running

To run R4 on foo.bar:

```
cd ~/R4/R4
./r4.sh foo.bar
```

The output is stored in `~/R4/R4/output/foo-bar`. To generate a report from the output into `~/R4/R4/report/foo-bar` issue the following command:

```
cd ~/R4/R4
./utils/batch-report/report.py website output report foo-bar
```
