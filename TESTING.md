Folding@Home Tester Guide
=========================

The document provides some information on running and testing the new
foldingathome.org client software code named "bastet".

## Install

 1. Uninstall the old FAH client.
 2. Download the client from https://foldingathome.org/alpha/
 3. Install the new FAH client.
 4. Open https://app.foldingathome.org/ in a browser to access the application.


## Differences between the old and new client software

The new client:

  * consists of two parts, a web frontend
    (https://github.com/FoldingAtHome/fah-web-client-bastet) and a backend
    (https://github.com/FoldingAtHome/fah-client-bastet).  The features
    of FAHViewer and FAHControl are or will be implemented in the frontend.
  * is [Open-Source](https://opensource.org/osd).
  * uses a new API to talk to the servers at foldingathome.org.
  * records a cryptographically signed record of all F@H credits earned.
  * no longer supports "slots".  Resources are allocated automatically.
  * will support future GPU + CPU hybrid work units.
  * will copy some of the configuration from the old client.
  * will not continue WUs left by the old client.
  * does not work with FAHControl.

## Communication

You can communicate with the developers and other testers either via GitHub
issues or using Slack.  Slack channel access is by invite only.

## Reporting bugs

Bugs can be reported on GitHub.  Please try to classify issues as frontend or
backend issues and file them in the appropriate issue tracker.  When creating
a GitHub issue:

### Choose a good title

Choose a title which briefly describes the problem so that reading the
title alone is enough to understand the issue.

Bad titles:

  * Problem on Windows
  * Won't install on my machine
  * Button doesn't work

Good titles:

  * Login fails on Edge browser in Windows
  * Missing ``openssl`` install dependency on Debian Linux
  * Cancel button in settings does not cancel save

### What to include in the description

Like the title, the description should be as brief as possible but this is where
you include all the important details.  Include the following in the
description:

  * What OS are you using?
  * What browser?
  * What did you expect to happen?
  * What happened instead?
  * A step-by-step method for repeating the problem.
  * Any related error messages you saw or found in the log files.

Now you can submit your issue.

### Follow up

After you've submitted an issue, be sure to follow up.  You may be asked to
provide more information, perform further testing or test out a fix.

## Running the client without the frontend

It is possible to run
[fah-client-bastet](https://github.com/FoldingAtHome/fah-client-bastet) without
the frontend.  You can then configure either by passing options on the
command line or using a ``config.xml`` file.  In Linux, there will be a
configuration file at ``/etc/fah-client/config.xml``.
Here is an example file:

```xml
<config>
  <user v="john.doe"/>
  <team v="12345"/>
  <passkey v="1234567890abcdef1234567890abcdef"/>
</config>
```

If manually running the client somewhere other than ``/var/lib/fah-client/``,
``config.xml`` should be in the current working directory.

Note that the ``config.xml`` configuration file will be overridden if a user
sets the configuration via the frontend.

In Linux, the ``fah-client`` service can be stopped and started using service
control commands.  In Windows, a system tray icon will appear when the client
is running.  This can be used to stop the client.

## Bleeding Edge Builds

The latest bleeding edge builds can be downloaded from
https://master.foldingathome.org/builds/fah-client/  These builds are not
official release and may not even be function.
They should be used for testing the latest pre-alpha code.
