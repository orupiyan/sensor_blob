<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

## Overview

This project is built on top of https://github.com/lance-proxy/mynewt_proj.
The purpose of this project is to collect samples of temperature measurement at every 100ms and create a blob containing the most recent ten samples and report this blob whenever there is a request at the GATT Service.
The collection and creation of blob is asynchronous. 
The reporting of data is based on the request from the GATT service.

## Building


1. Download and install Apache Newt.

You will need to download the Apache Newt tool, as documented in the [Getting Started Guide](http://mynewt.apache.org/os/get_started/introduction/).

2. Download the Apache Mynewt Core package (executed from the ble_sensor directory).

```no-highlight
    $ newt install
```

3. Build the ble_sensor app for the sim platform using the "ble_sensor" target
(executed from the ble_sensor directory).

```no-highlight
    $ newt build ble_sensor
```

4. Create the image to load on to the target
(executed from the ble_sensor directory).

```no-highlight
    $ newt create-image ble_sensor 1.0.0
```

5. Load the target
(executed from the ble_sensor directory).

```no-highlight
    $ newt load ble_sensor
```
