#!/bin/sh
export LC_ALL=C
NPOINTS=${1:-200000}
RUNS=${2:-10}

make -s mandelbrot || exit 1
echo "cpu: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || uname -m)," \
     "cores: $(sysctl -n hw.ncpu 2>/dev/null)" \
     "(P: $(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null)," \
     "E: $(sysctl -n hw.perflevel1.physicalcpu 2>/dev/null))"
echo "n_points=$NPOINTS runs=$RUNS"
echo "p,min_s,avg_s,max_s,S,E"
T1=""
for P in 1 2 4 6 8 10 12 16; do
   LINE=$(for R in $(seq "$RUNS"); do
      ./mandelbrot "$P" "$NPOINTS" | awk '/время работы/ {print $4}'
   done | awk 'NR==1 || $1<min {min=$1} NR==1 || $1>max {max=$1} {sum+=$1}
               END {printf "%.4f %.4f %.4f", min, sum/NR, max}')
   set -- $LINE
   [ -z "$T1" ] && T1=$1
   awk -v p="$P" -v mn="$1" -v av="$2" -v mx="$3" -v t1="$T1" \
      'BEGIN {s = t1/mn; printf "%d,%s,%s,%s,%.2f,%.2f\n", p, mn, av, mx, s, s/p}'
done
