Folding@Home Desktop Client
===========================

Folding@home is a distributed computing project -- people from
throughout the world download and run software to band together to
make one of the largest supercomputers in the world. Every computer
takes the project closer to our goals. Folding@home uses novel
computational methods, coupled to distributed computing, to simulate
problems millions of times more challenging than previously achieved.

Protein folding is linked to disease, such as Alzheimer's, ALS,
Huntington's, Parkinson's disease, and many Cancers.

Moreover, when proteins do not fold correctly (i.e. "misfold"), there
can be serious consequences, including many well known diseases, such
as Alzheimer's, Mad Cow (BSE), CJD, ALS, Huntington's, Parkinson's
disease, and many Cancers and cancer-related syndromes.

# What is protein folding?
Proteins are biology's workhorses -- its "nanomachines." Before
proteins can carry out these important functions, they assemble
themselves, or "fold." The process of protein folding, while critical
and fundamental to virtually all of biology, in many ways remains a
mystery.

# This software
This repository contains a new [Open-Source](https://opensource.org/osd)
version of the Folding@home client software.  The complete client software
consists of a frontend and a backend.  This repository contains the backend.
The frontend is in a separate repository at
[fah-web-client-bastet](https://github.com/foldingathome/fah-web-client-bastet).
The backend can be configured to run on its own without any user interaction.
The frontend is a web application which normally will run at
https://app.foldingathome.org/ but can also be run locally for testing and
development purposes.

# Quick Start for Debian Linux

(see the [BUILDING-RPM.md](BUILDING-RPM.md) file for instructions on how to build the RPM package)

## Install the Prerequisites
```
sudo apt update
sudo apt install -y scons git npm build-essential libssl-dev zlib1g-dev libbz2-dev libsystemd-dev
```

## Get the code
```
git clone https://github.com/cauldrondevelopmentllc/cbang
git clone https://github.com/foldingathome/fah-client-bastet
git clone https://github.com/foldingathome/fah-web-client-bastet
```

## Build the Folding@home Client
```
export CBANG_HOME=$PWD/cbang
scons -C cbang
scons -C fah-client-bastet
scons -C fah-client-bastet package
```

## Install the package
The last build step builds the Debian package.  You can then install it like this:

```
sudo apt install ./fah-client-bastet/fah-client_<version>_amd64.deb
```

Where ``<version>`` is the software version number.

Folding@home Client older than v8 will be automatically removed.

## Folding@home Client Service
After installation, the service runs and will automatically restart on startup.

**File storage locations:**
- Logs: `/var/log/fah-client`
- Data: `/var/lib/fah-client`

Related service commands for **Status, Start, Stop, Restart:**
```
systemctl status --no-pager -l fah-client
sudo systemctl start fah-client
sudo systemctl stop fah-client
sudo systemctl restart fah-client
```

NOTE: If the Folding@home Client is not being run as a service and is manually
run with `fah-client` in a terminal window, the data and log folders will be
created in the working directory where it is run from.

## Start the development web server
Use these commands to run your own frontend server for testing purposes.  In
production, this code will run at https://app.foldingathome.org/.

```
cd fah-web-client-bastet
npm i
npm run dev
```

With the development server running, open http://localhost:3000/ in a browser to
view the client frontend.
