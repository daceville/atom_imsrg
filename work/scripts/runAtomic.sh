#!/bin/bash

# decide if we should run in the current shell or submit to the cluster
# interactive=true means run in the current shell.
interactive=false
NTHREADS=12
WORKDIR=$PWD

exe=/home/dlivermore/ragnar_imsrg/work/compiled/Atomic
#exe=/global/home/dlivermore/imsrg/work/compiled/writeAtomicTBME

vnn=
file2e1max=6
file2e2max=12
file2lmax=6
v3n=none

estart=10
estop=10
eiter=2

lstart=0
lstop=0
liter=1

hwstart=110
hwstop=110
hwiter=5

#for ((A=$start;A<=$stop;A++)); do
for ((emax=$estart;emax<=$estop;emax=emax+eiter)); do
for ((Lmax=$lstart; Lmax<=$lstop; Lmax=Lmax+liter)); do
for ((hw=$hwstart; hw<=$hwstop; hw=hw+hwiter)); do
#for ((Lmax=$lstart;Lmax<=$lstop && Lmax<=$emax;Lmax=Lmax+liter)); do
#A=4
#state=10
systemtype=atomic
# 1.0 Hartree ~= 27.21138505(60) eV (according to Wikipedia)
#hw=27.21138505 # Only matters in HO/Slater
#hw=40.817077575 # 1.5*H
#hw=13.605692525 # 0.5*H
#hw=108.8
#hw = 1
valence_space=He4
reference=He4
#systemBasis=hydrogen
systemBasis=harmonic
smax=200
#emax=4
#Lmax=2
e3max=0
lmax3=2
method=magnus
basis=HF
omega_norm_max=0.25
#file3='file3e1max=12 file3e2max=28 file3e3max=12'
mode=batchmpi
#mode=debug

#jobname="method_${method}_ref_${reference}_basis_${systemBasis}_emax_${emax}_lmax_${Lmax}"
#jobname="ref_${reference}_basis_${systemBasis}_emax_${emax}_hw_${hw}"
jobname="ref_${reference}_basis_${systemBasis}_emax_${emax}_lmax_${lmax}"

#Operators=KineticEnergy,InverseR,CorrE2b
#Operators=KineticEnergy,InverseR,ElectronTwoBody
Operators=
#scratch=

all_the_flags="emax=${emax} e3max=${e3max} method=${method} valence_space=${valence_space} hw=${hw} smax=${smax} omega_norm_max=${omega_norm_max} reference=${reference} Operators=${Operators} Lmax=${Lmax} systemtype=${systemtype} use_brueckner_bch=false file2e1max=${file2e1max} file2e2max=${file2e2max} file2lmax=${file2lmax} 2bme=${vnn} systemBasis=${systemBasis}"

#command="valgrind -v ${exe} ${all_the_flags}"
#command="valgrind --trace-children=yes --leak-check=full --track-origins=yes -v ${exe} ${all_the_flags}"
# --read-inline-info=yes # This looks like it would be awesome.
command="${exe} ${all_the_flags}"
echo command is $command

if [ $interactive = 'true' ]; then
echo "Running interactive"
cd $WORKDIR
export OMP_NUM_THREADS=${NTHREADS}
$command
else
echo jobname is ${jobname}
echo "running"

#PBS -N ${jobname}
#PBS -q batchmpi
#PBS -d /home/dlivermore/ragnar_imsrg/work/scripts/
#PBS -l walltime=196:00:00
#PBS -l nodes=1:ppn=12
#PBS -l vmem=60gb
#PBS -m abe
#PBS -M dlivermore@triumf.ca
#PBS -j oe
#PBS -e pbslog/${jobname}.e.`date +"%g%m%d%H%M"`
#PBS -o pbslog/${jobname}.o.`date +"%g%m%d%H%M"`
#PBS -e pbslog/
#PBS -o pbslog/

cd $WORKDIR
export OMP_NUM_THREADS=${NTHREADS}
qsub ${jobname}
#$command
fi

done
done
done

# set the command with all the flags, commented out as I already have
#command=${exe} ${all_the_flags}

