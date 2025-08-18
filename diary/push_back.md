## `push_back`のループがちゃんと消えてた話
### 2025-08-18
書きながらこれほんとに消えるのかなと思っていた`push_back`の`std::index_sequence<Is...>`とラムダを用いたループのところちゃんと消えててよかった
```asm
00000001000029c0 <__ZZN2kk13static_bufferINS_9generator4uuidENS_11eventsystem5dummyELm4EJiiEE9push_backEOiS6_ENKUlTpTnmNSt3__116integer_sequenceImJXspT_EEEEE_clIJLm0ELm1EEEEDaS9_>:
1000029c0: d10103ff     sub     sp, sp, #64
1000029c4: a9037bfd     stp     x29, x30, [sp, #48]
1000029c8: 9100c3fd     add     x29, sp, #48
1000029cc: f81f03a0     stur    x0, [x29, #-16]
1000029d0: f85f03a9     ldur    x9, [x29, #-16]
1000029d4: f90007e9     str     x9, [sp, #8]
1000029d8: f9400128     ldr     x8, [x9]
1000029dc: f9000be8     str     x8, [sp, #16]
1000029e0: f9400529     ldr     x9, [x9, #8]
1000029e4: b9400129     ldr     w9, [x9]
1000029e8: b90007e9     str     w9, [sp, #4]
1000029ec: 91012100     add     x0, x8, #72
1000029f0: 94000446     bl      0x100003b08 <__ZNSt3__13getB8ue170006ILm0EJNS_5arrayIiLm4EEES2_EEERNS_13tuple_elementIXT_ENS_5tupleIJDpT0_EEEE4typeERS7_>
1000029f4: f9400be8     ldr     x8, [sp, #16]
1000029f8: f9400101     ldr     x1, [x8]
1000029fc: 9400044c     bl      0x100003b2c <__ZNSt3__15arrayIiLm4EEixB8ue170006Em>
100002a00: b94007ea     ldr     w10, [sp, #4]
100002a04: f94007e9     ldr     x9, [sp, #8]
100002a08: f9400be8     ldr     x8, [sp, #16]
100002a0c: b900000a     str     w10, [x0]
100002a10: f9400929     ldr     x9, [x9, #16]
100002a14: b9400129     ldr     w9, [x9]
100002a18: b81ec3a9     stur    w9, [x29, #-20]
100002a1c: 91012100     add     x0, x8, #72
100002a20: 9400044b     bl      0x100003b4c <__ZNSt3__13getB8ue170006ILm1EJNS_5arrayIiLm4EEES2_EEERNS_13tuple_elementIXT_ENS_5tupleIJDpT0_EEEE4typeERS7_>
100002a24: f9400be8     ldr     x8, [sp, #16]
100002a28: f9400101     ldr     x1, [x8]
100002a2c: 94000440     bl      0x100003b2c <__ZNSt3__15arrayIiLm4EEixB8ue170006Em>
100002a30: b85ec3a8     ldur    w8, [x29, #-20]
100002a34: b9000008     str     w8, [x0]
100002a38: a9437bfd     ldp     x29, x30, [sp, #48]
100002a3c: 910103ff     add     sp, sp, #64
100002a40: d65f03c0     ret
```