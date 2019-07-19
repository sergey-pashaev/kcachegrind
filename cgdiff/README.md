# Motivation

Some time ago I did a lot of work with KCachegrind & used it
intensively. I often needed an instrument that could compare different
callgrind outputs, to see changes in function's amount of executed
instructions and it was hard to do with GUI (I hope some day
KCachegrind will be able to compare different callgrind profiles). And
I didn't found any decent tool to do these comparisons (there is
cg_diff for cachegrind but not for callgrind and there is
callgrind_diff.sh in valgrind repo but it is lacking of
functionality).

So I decided to write cgdiff tool similar to cgview.

# Usage examples

## usage
```
$ cgdiff -h
Compare profiles from callgrind files. (c) 2017 S. Pashaev
Usage: cgdiff [options] <file> ...

Options:
 -h        Show this help text
 -n        Do not detect recursive cycles
 -m <num>  Show only function with <num> minimal delta value
 -s <ev>   Show counters for event <ev>
 -f <str>  Show only functions with <str> in name
 -e        Sort by exclusive cost change
 -c        Sort by call count change
 -r        Sort percentage change
 -p        Use per call counters
 -t        Show templates
```

## simplest case, details for Ir by default
```
$ cgdiff callgrind.out.31976 callgrind.out.31982
A = callgrind.out.31976
B = callgrind.out.31982

Totals for event types:
             A                           B                       B - A                  +/- %         Event type
        696533                     1341606                      645073                 +92.61         Instruction Fetch (Ir)
        165752                      307509                      141757                 +85.52         Data Read Access (Dr)
         69197                      149488                       80291                +116.03         Data Write Access (Dw)
          1835                       12450                       10615                +578.47         L1 Instr. Fetch Miss (I1mr)
          4318                        6493                        2175                 +50.37         L1 Data Read Miss (D1mr)
          1328                        1864                         536                 +40.36         L1 Data Write Miss (D1mw)
          1684                        2985                        1301                 +77.26         LL Instr. Fetch Miss (ILmr)
          2800                        3401                         601                 +21.46         LL Data Read Miss (DLmr)
          1204                        1528                         324                 +26.91         LL Data Write Miss (DLmw)
        123647                      230419                      106772                 +86.35         Conditional Branch (Bc)
          9908                       19618                        9710                 +98.00         Mispredicted Cond. Branch (Bcm)
          4991                        9132                        4141                 +82.97         Indirect Branch (Bi)
           569                        1895                        1326                +233.04         Mispredicted Ind. Branch (Bim)

Common functions:
Sorted by: change in Inclusive Instruction Fetch (Ir)
Filtered by: minimal value: '1'
                    A                     B                 B - A           +/-% / function name
               696536               1341609               +645073         +92.61 / 0x0000000000000c30
               390185               1035210               +645025        +165.31 / (below main)
               469267               1114292               +645025        +137.45 / _dl_runtime_resolve_avx
               391323               1036348               +645025        +164.83 / 0x00000000004049a0
               380462               1023330               +642868        +168.97 / 0x0000000000402a00
               165968                576499               +410531        +247.36 / _dl_runtime_resolve_avx'2
                12646                344497               +331851       +2624.16 / 0x0000000000407f80

... skipped ...

                   12                    18                    +6         +50.00 / fileno
                63243                 63249                    +6          +0.01 / setlocale
                 3019                  3014                    -5          -0.17 / handle_ld_preload
                    2                     6                    +4        +200.00 / _dl_debug_state

              5704656              10560556              +4855900         +85.12 / TOTAL

A functions (not in B):
Sorted by: Inclusive Instruction Fetch (Ir)
Filtered by: minimal value: '1'
           call count                 value / function name
                   72                 83097 / 0x0000000000405d50
                    1                 80762 / 0x0000000000405ef0
                   36                 51398 / 0x0000000000406a30
                   33                  8705 / 0x0000000000405310
                    1                  2538 / 0x000000000040cc10
                    1                  1570 / 0x0000000000411630

                                     228070 / TOTAL

B functions (not in A):
Sorted by: Inclusive Instruction Fetch (Ir)
Filtered by: minimal value: '1'
           call count                 value / function name
                   39                333287 / 0x0000000000406e60
                   39                191280 / 0x0000000000407eb0
                   78                183753 / 0x000000000040cdf0
                    1                180115 / getpwuid

... skipped ...

                    1                179762 / getpwuid_r@@GLIBC_2.2.5
                   39                127680 / localtime
                   39                127563 / __tz_convert
                    1                     3 / 0x00000000062e62a4
                    1                     3 / 0x0000000005cb6bf4
                    1                     3 / 0x00000000060da8ac
                    1                     3 / 0x0000000005eca1e4

                                    4144754 / TOTAL
```

## show per-call relative changes
```
$ cgdiff -p -r callgrind.out.31976 callgrind.out.31982
A = callgrind.out.31976
B = callgrind.out.31982

Totals for event types:
             A                           B                       B - A                  +/- %         Event type
        696533                     1341606                      645073                 +92.61         Instruction Fetch (Ir)
        165752                      307509                      141757                 +85.52         Data Read Access (Dr)
         69197                      149488                       80291                +116.03         Data Write Access (Dw)
          1835                       12450                       10615                +578.47         L1 Instr. Fetch Miss (I1mr)
          4318                        6493                        2175                 +50.37         L1 Data Read Miss (D1mr)
          1328                        1864                         536                 +40.36         L1 Data Write Miss (D1mw)
          1684                        2985                        1301                 +77.26         LL Instr. Fetch Miss (ILmr)
          2800                        3401                         601                 +21.46         LL Data Read Miss (DLmr)
          1204                        1528                         324                 +26.91         LL Data Write Miss (DLmw)
        123647                      230419                      106772                 +86.35         Conditional Branch (Bc)
          9908                       19618                        9710                 +98.00         Mispredicted Cond. Branch (Bcm)
          4991                        9132                        4141                 +82.97         Indirect Branch (Bi)
           569                        1895                        1326                +233.04         Mispredicted Ind. Branch (Bim)

Common functions:
Sorted by: percentage change in Inclusive Instruction Fetch (Ir) (per call)
Filtered by: minimal value: '1'
                    A                     B                 B - A           +/-% / function name
                   24                   770                  +746       +3108.33 / __GI_strstr
                  351                  8833                 +8482       +2416.52 / 0x0000000000407f80
               380462               1023330               +642868        +168.97 / 0x0000000000402a00
               390185               1035210               +645025        +165.31 / (below main)
               391323               1036348               +645025        +164.83 / 0x00000000004049a0
                36097                 85714                +49617        +137.45 / _dl_runtime_resolve_avx
                 2345                  5022                 +2677        +114.16 / _dl_catch_error
                   23                    45                   +22         +95.65 / __GI_strchr
                  335                    41                  -294         -87.76 / _IO_setb
                  320                    45                  -275         -85.94 / _IO_default_xsputn
               183132                339510               +156378         +85.39 / 0x0000000000407790
                61602                 10822                -50780         -82.43 / __fopen_internal
                61604                 10824                -50780         -82.43 / fopen@@GLIBC_2.2.5

... skipped ...

                  574                   559                   -15          -2.61 / do_lookup_x
                  771                   790                   +19          +2.46 / _dl_lookup_symbol_x
                   43                    42                    -1          -2.33 / __GI_strncmp
                  560                   573                   +13          +2.32 / 0x0000000000405170
                   93                    95                    +2          +2.15 / _dl_name_match_p
                 2507                  2560                   +53          +2.11 / _dl_init_paths
                  175                   177                    +2          +1.14 / readdir

              1976757               3949727              +1972970         +99.81 / TOTAL

A functions (not in B):
Sorted by: Inclusive Instruction Fetch (Ir) (per call)
Filtered by: minimal value: '1'
           call count                 value / function name
                    1                 80762 / 0x0000000000405ef0
                    1                  2538 / 0x000000000040cc10
                    1                  1570 / 0x0000000000411630
                   36                  1427 / 0x0000000000406a30
                   72                  1154 / 0x0000000000405d50
                   33                   263 / 0x0000000000405310

                                      87714 / TOTAL

B functions (not in A):
Sorted by: Inclusive Instruction Fetch (Ir) (per call)
Filtered by: minimal value: '1'
           call count                 value / function name
                    1                180115 / getpwuid
                    1                179762 / getpwuid_r@@GLIBC_2.2.5
                    1                109904 / _nss_compat_getpwuid_r
                    1                 69308 / __nss_passwd_lookup2
                    1                 59082 / read_alias_file

... skipped ...

                    1                     3 / 0x0000000005eca1e4
                    1                     3 / 0x0000000005cb6bf4
                    1                     3 / __syscall_clock_gettime
                  107                     3 / __ctype_b_loc
                    1                     3 / 0x00000000062e62a4
                    1                     3 / 0x00000000060da8ac

                                    1273517 / TOTAL
```

Also, it is possible to filter functions by name, set minimal threshold value to filter out unsignificant changes, sort by call count changes, etc.
