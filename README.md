# Arduino-Inseparates
Component audio system communication library:\
Making your HiFi separates a little less separate

## Features
* Complements IR libraries by providing support for protocols and features specific to component audio interconnects.
* Supports multiple concurrent send and receives on multiple pins.
* Supports single pin IO.
* Supports multi pin protocols.

## Supported Protocols
IC = Interconnect
* NEC (IR/IC)
* RC5 (IR/IC)
* SIRC (IR/IC)
* Bang & Olufsen 36 kHz (IR)
* Bang & Olufsen Datalink 80 (IC)
* Bang & Olufsen Datalink 86 (IR/IC)
* Philips ESI (IC)
* Technics System Control (IC)
* RS232

Due to the parallel on-the-fly decoding and encoding this library cannot simultaneously support as many protocols as a traditional IR library can.\
To support more IR protocols this library can be used together with an IR library as demonstrated in some of the examples.

## Use cases
* Connect different brands.
* Home automation.
* App remotes.
* DIY preamps/controllers.

## Implementation Details

## Interconnect Protocols
