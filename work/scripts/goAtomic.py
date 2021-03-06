#!/usr/bin/env python2

##########################################################################
##  goAtomic.py
##
##  A python script to run or submit jobs for the common use cases
##  of the IMSRG++ code. We check whether there is a pbs or slurm
##  scheduler, assign the relevant input parameters, set names
##  for the output files, and run or submit.
##  						-Ragnar Stroberg
##  						TRIUMF Nov 2016
######################################################################

from os import path,environ,mkdir,remove
from sys import argv
from subprocess import call,PIPE
from time import time,sleep
from datetime import datetime
import argparse
import csv
import random
import numpy as np

csv_fn = "data_log.csv"

parser = argparse.ArgumentParser()
parser.add_argument("-c","--comment", help="Add a comment to be logged", type=str)
py_args = parser.parse_args()
print("This is the comment: {}".format(py_args.comment))


### Check to see what type of batch submission system we're dealing with
BATCHSYS = 'NONE'
if call('type '+'qsub', shell=True, stdout=PIPE, stderr=PIPE) == 0: BATCHSYS = 'PBS'
elif call('type '+'srun', shell=True, stdout=PIPE, stderr=PIPE) == 0: BATCHSYS = 'SLURM'

### The code uses OpenMP and benefits from up to at least 24 threads
NTHREADS=32
#exe = '/global/home/dlivermore/imsrg/work/compiled/writeAtomicTBME'#%(environ['HOME'])
#exe = '/global/home/dlivermore/imsrg_backup/work/compiled/Atomic'  # Oak
exe = '/home/dlivermo/projects/def-holt/dlivermo/atom_imsrg/work/compiled/Atomic' # Cedar
#exe = '/home/dlivermore/ragnar_imsrg/work/compiled/Atomic' # Cougar

### Flag to switch between submitting to the scheduler or running in the current shell
#batch_mode=False
batch_mode=True
if 'terminal' in argv[1:]: batch_mode=False

### Don't forget to change this. I don't want emails about your calculations...
mail_address = 'dlivermore@triumf.ca'

### This comes in handy if you want to loop over Z
ELEM = ['n','H','He','Li','Be','B','C','N',
       'O','F','Ne','Na','Mg','Al','Si','P','S','Cl','Ar','K',
       'Ca','Sc','Ti','V','Cr','Mn','Fe','Co',  'Ni','Cu','Zn','Ga','Ge','As','Se','Br','Kr','Rb','Sr','Y',
       'Zr','Nb','Mo','Tc','Ru','Rh','Pd','Ag','Cd','In',  'Sn','Sb','Te','I','Xe','Cs','Ba','La','Ce','Pr','Nd','Pm','Sm','Eu','Gd','Tb','Dy','Ho','Er','Tm','Yb',
       'Lu','Hf','Ta','W','Re','Os','Ir','Pt','Au','Hg','Tl','Pb']# ,'Bi','Po','At','Rn','Fr','Ra','Ac','Th','U','Np','Pu']

### ARGS is a (string => string) dictionary of input variables that are passed to the main program
ARGS  =  {}

### Maximum value of s, and maximum step size ds
ARGS['smax'] = '200'
ARGS['dsmax'] = '0.5'

#ARGS['lmax3'] = '10' # for comparing with Heiko

### Norm of Omega at which we split off and start a new transformation
ARGS['omega_norm_max'] = '0.25'

### Model space parameters used for reading Darmstadt-style interaction files
#ARGS['file2e1max'] = '14 file2e2max=28 file2lmax=10'
#ARGS['file3e1max'] = '14 file3e2max=28 file3e3max=14'
ARGS['file2e1max'] = 6
ARGS['file2e2max'] = 12
ARGS['file2lmax'] = 6

### Name of a directory to write Omega operators so they don't need to be stored in memory. If not given, they'll just be stored in memory.
#ARGS['scratch'] = '/home/dlivermo/scratch'

### Generator for core decoupling, can be atan, white, imaginary-time.  (atan is default)
#ARGS['core_generator'] = 'imaginary-time' 
#ARGS['core_generator'] = 'white'
ARGS['core_generator'] = 'atan'

### Generator for valence deoupling, can be shell-model, shell-model-atan, shell-model-npnh, shell-model-imaginary-time (shell-model-atan is default)
#ARGS['valence_generator'] = 'shell-model-imaginary-time' 
#ARGS['valence_generator'] = ARGS['core_generator']
ARGS['valence_generator'] = 'shell-model-atan'

### Solution method
ARGS['method'] = 'magnus'
#ARGS['method'] = 'magnus_modified_euler'
#ARGS['method'] = 'brueckner'
#ARGS['method'] = 'flow'
#ARGS['method'] = 'HF'
#ARGS['method'] = 'MP3'

### Tolerance for ODE solver if using flow solution method
ARGS['ode_tolerance'] = '1e-5'

###
ARGS['Operators'] = '' #'Trel_Op,InverseR,KineticEnergy,TCM_Op'

### Create the 'script' that we need for execution
if BATCHSYS is 'PBS':
	FILECONTENT = """#!/bin/bash
#PBS -N %s
#PBS -q oak
#PBS -d %s
#PBS -l walltime=72:00:00
#PBS -l nodes=1:ppn=%d
#PBS -l vmem=250gb
#PBS -m abe
#PBS -M %s
#PBS -j oe
#PBS -o pbslog/%s.o
#PBS -e pbslog/%s.e
cd $PBS_O_WORKDIR
export OMP_NUM_THREADS=%d
%s
"""
	
elif BATCHSYS is 'SLURM':
	FILECONTENT = """#!/bin/bash
#SBATCH --account=rrg-holt
#SBATCH --time=8:00:00
#SBATCH --mem=30G
#SBATCH --job-name={0}
#SBATCH --output={1}/pbslog/{2}
#SBATCH --cpus-per-task={3}
#SBATCH --mail-user=dlivermore@triumf.ca
#SBATCH --mail-type=END
{4}
"""

### Loop parameters
#batch_mode = True
ARGS['fmt2'] = 'lv2'
e_start=4
e_stop =8
ARGS['denominator_delta'] = 0

e_iter =2

l_start=0
l_stop =0
l_iter =1

hwstart=2
hwstop =12
hwiter =1
hwN    =6
hw_vec = np.linspace(hwstart, hwstop, hwN)

time_string = "{}".format( datetime.fromtimestamp(time()).strftime('_%y%m%d%H%M.log' ) )

### Loops!
for emax in range(e_start,e_stop+1,e_iter):
	for Lmax in range(l_start,l_stop+1,l_iter):
		for hw in hw_vec:
			ARGS['hw'] = str(hw) # Cast as strings, just incase shenanigans ensue
			ARGS['Lmax'] = str(Lmax)
			ARGS['emax'] = str(emax)
			ARGS['valence_space'] 	= 'Kr84'
			ARGS['reference'] 	= 'Kr84'
			#ARGS['systemBasis']	= 'hydrogen'
			ARGS['systemBasis']	= 'harmonic'
			ARGS['smax']		= '200'
			#ARGS['method']		= 'magnus'
			ARGS['basis']		= 'HF'
			ARGS['omega_norm_max']	= '0.25'
			ARGS['e3max']		= '0'
			#ARGS['account']	= 'rrg-holt'
			#ARGS['2bme']		= '/global/scratch/dlivermore/ME_emax16_hw1_Apr17_2019v2.me2j'
			#ARGS['2bme']		= '/home/dlivermo/scratch/ME_emax4_whatever'
			ARGS['2bme']		= '/home/dlivermo/projects/def-holt/dlivermo/ME/ME_Laguerre_emax8_hw1_May29_2019.lv2'
			if ARGS['systemBasis'] == 'hydrogen':
				jobname		= "ref_{0}_val_{4}_basis_{1}_emax_{2}_lmax_{3}".format(ARGS['reference'],ARGS['systemBasis'],emax,lmax,ARGS['valence_space'])
			elif ARGS['systemBasis'] == 'harmonic':
				jobname		= "ref_{0}_val_{4}_basis_{1}_emax_{2}_hw_{3}".format(ARGS['reference'],ARGS['systemBasis'],emax,hw,ARGS['valence_space'])
			"""all_the_flags		= "emax={0} e3max={1} method={2} valence_space={3} hw={4} smax={5} omega_norm_max={6}
						   reference={7} Operators={8} Lmax={9} file2e1max={10} file2e2max={11} file2lmax={12}
						   2bme={13} systemBasis={14}  systemtype={15} use_brueckner_bch=false".format(
						   emax,ARGS['e3max'],ARGS['method'],ARGS['valence_space'],hw,ARGS['smax'],ARGS['omega_norm_max'],
						   ARGS['reference'],ARGS['Operators'],lmax,ARGS['file2e1max'],ARGS['file2e2max'],ARGS['file2lmax'],
						   '',ARGS['systemBasis'],'Atomic') """

			logname = jobname + time_string
			cmd = ' '.join([exe] + ['%s=%s'%(x,ARGS[x]) for x in ARGS])
			if batch_mode==True:
				print "Submitting to cluster: jobname=" + jobname+".batch"
				for c in cmd.split():
					print c
				if BATCHSYS is 'PBS':
					sfile = open(jobname+'.batch','w')
					sfile.write(FILECONTENT%(jobname,environ['PWD'],NTHREADS,mail_address,logname,logname,NTHREADS,cmd))
					sfile.close()
				elif BATCHSYS is 'SLURM':
					sfile = open(jobname+'.batch','w')
                                        sfile.write(FILECONTENT.format(jobname,environ['PWD'],logname,NTHREADS,cmd))
                                        sfile.close()
				if path.isfile(jobname+'.batch'):
					if BATCHSYS is 'PBS':
						call(['qsub', jobname+'.batch'])
					elif BATCHSYS is 'SLURM':
						#call(['chmod', '+x', jobname+'.batch'])
						#cmd = 'sbatch ' + cmd
						call(['sbatch', jobname+'.batch'])
				else:
					print("Unable to locate .batch file!")
			else:
				if BATCHSYS is 'PBS':
					for c in cmd.split():
						print(c)
					call(cmd.split())
				elif BATCHSYS is 'SLURM':
					call(['srun', jobname+'.batch'])
			with open(csv_fn, 'a+') as csvfile:
				csv_writer = csv.writer(csvfile, delimiter=',',
                            		quotechar='|', quoting=csv.QUOTE_MINIMAL)
				if py_args.comment:
 					csv_writer.writerow([datetime.fromtimestamp(time()).strftime('%y:%m:%d:%H:%M'),cmd,py_args.comment])
				else:
					csv_writer.writerow([datetime.fromtimestamp(time()).strftime('%y:%m:%d:%H:%M'),cmd,py_args.comment])

