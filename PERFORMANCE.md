# Performance measurements  

This does just so happen to be relevant as this is designed to run for a LONG time if you wish to.  
I'll be taking measurements and denote the version I'm working with per measurement.  

These are the specs of the machine I'm testing on:

```plaintext
Microslop Windows 11 Enterprise 10.0.26200
Dell Latitude 7440
13th Gen Intel(R) Core(TM) i7-1365U, 1800 Mhz, 10 Core 
16GB RAM, 35GB Swap
Intel Iris Xe Integrated Graphics
256GB Samsung NVMe SSD PM9B1
```

## Endurance test 1  

Testing occurred over one weekend. Program was started and left running in the background.  
Total time: ~68 hours  
Active time: ~6 hours  
App version: 0.1.0  

One measurement was taken at the end of this time, before a restart to capture the initial state.  
Both measurement were taken using Process Hacker.  

|               At 68 hours               |               At 0 hours              |
|:---------------------------------------:|:-------------------------------------:|
| ![Test 1 - 68 hours](./imgs/t1_68h.png) | ![Test 1 - 0 hours](./imgs/t1_0h.png) |

The most noticable change is an increased memory usage - however this is not a significant change at this time and should be tested further before any conclusions are drawn.  
Also of note is a stable low CPU usage and generally low I/O and Memory usage.  

Looking at the log (8,72kB in this case); It has indeed not been running while the device was sleeping. A sharp cut appears from 24.04.2026 to 27.04.2026 - quite as expected.  
Regardless of the exceedingly expected limitations of OS inactivity, the cature seems accurate for all events occurring within the given timeframe, including detecting that the device was reactivated (in a sudden logging spree from the sudden change)  

Interestingly enough, it reveals when the applicable fileshares go offline, as a network disturbance seems to have occurred before the device initiated sleep.  

A new test with a device remaining active for 72 hours is recommended.  
