#!/usr/bin/env python2.7

import matplotlib
matplotlib.use('Agg') # otherwise script does not work when invoked over SSH
from cycler import cycler
import matplotlib.pyplot as plt
import matplotlib.ticker as plticker
import pandas
import sys
import time
import numpy as np

setTitle=False

if len(sys.argv) < 5:
    print "Usage:", sys.argv[0], "<output-png-file> <dkgs-allowed> <min-N> <max-N> <csv-file> [<csv-file>] ..."
    sys.exit(0)

del sys.argv[0]

out_png_file = sys.argv[0]
del sys.argv[0]

dkgs_allowed = sys.argv[0].split(",")
del sys.argv[0]

minN = int(sys.argv[0])
del sys.argv[0]

maxN = int(sys.argv[0])
del sys.argv[0]

if not out_png_file.endswith('.png'):
    print "ERROR: Expected .png file as first argument"
    sys.exit(1)

data_files = [f for f in sys.argv]

print "Reading CSV files:", data_files, "..."

csv_data = pandas.concat((pandas.read_csv(f) for f in data_files))

#print "Raw:"
#print csv_data.to_string()
#print csv_data.columns
#print csv_data['dictSize'].values

#print "Averaged:"

if minN == 0:
    minN = csv_data.n.unique().min();
if maxN == 0:
    maxN = csv_data.n.unique().max();

print "min N:", minN
print "max N:", maxN

# we specify the DKGs here in a specific order so they are plotted with the right colors
dkgs = [ "eJF-DKG", "AMT DKG", "JF-DKG" ]
csv_data.dkg.replace('Feldman', 'JF-DKG', inplace=True)
csv_data.dkg.replace('Kate et al', 'eJF-DKG', inplace=True)
csv_data.dkg.replace('AMT', 'AMT DKG', inplace=True)

#csv_data = csv_data[csv_data.dkg != 'Feldman']
csv_data = csv_data[csv_data.n >= minN]
csv_data = csv_data[csv_data.n <= maxN]

csv_data.download_bw_bytes /= 1024*1024    # bytes to MB
csv_data.upload_bw_bytes /= 1024*1024    # bytes to MB

print csv_data.to_string()  # print all data

#SMALL_SIZE = 10
MEDIUM_SIZE = 20
BIG_SIZE = 24
BIGGER_SIZE = 28
BIGGEST_SIZE = 32

plt.rc('lines', linewidth=3, markersize=8, markeredgewidth=3)   # width of graph line & size of marker on graph line for data point
plt.rc('font',   size=      BIGGER_SIZE)    # controls default text sizes
#plt.rc('axes',   titlesize= BIGGER_SIZE)    # fontsize of the axes title
#plt.rc('axes',   labelsize= MEDIUM_SIZE)    # fontsize of the x and y labels
plt.rc('xtick',  labelsize= BIG_SIZE)
plt.rc('ytick',  labelsize= BIG_SIZE)
plt.rc('legend', fontsize=  BIGGER_SIZE)
#plt.rc('figure', titlesize=BIGGER_SIZE)     # fontsize of the figure title

#plt.rcParams['axes.prop_cycle'] = cycler(color='bgrcmyk')
plt.rcParams['axes.prop_cycle'] = cycler(color=[
    '#1f77b4', # blue
    '#1f77b4', # blue
    '#ff7f0e', # orange
    '#ff7f0e', # orange
    ])

print "DKGs in file:", csv_data.dkg.unique()
print "DKGs known:  ", dkgs

def plotNumbers(data):
    x = data.n.unique()    #.unique() # x-axis is the number of total players n

    #logBase = 10
    logBase = 2
    
    print "Plotting with log base", logBase 

    # adjust the size of the plot: (20, 10) is bigger than (10, 5) in the
    # sense that fonts will look smaller on (20, 10)
    fig, ax1 = plt.subplots(figsize=(12,7.5))
    ax1.set_xscale("log", basex=logBase)
    ax1.set_yscale("log")
    #ax1.set_xlabel("Total # of players n")
    if setTitle:
        ax1.set_title('DKG per-player bandwidth') #, fontsize=fontsize)
    #ax1.set_ylabel("Bandwidth (in MB)") #, fontsize=fontsize)
    ax1.grid(True)
    
    plots1 = []
    names1 = []
    
    #plt.xticks(x, x, rotation=30)
    for dkg in dkgs:
        if dkg not in dkgs_allowed:
            print "Skipping over disallowed:", dkg
            continue

        filtered = data[data.dkg == dkg]
        col1 = filtered.download_bw_bytes.values

        assert len(x) == len(col1)
        plt1, = ax1.plot(x, col1, linestyle="-", marker="o")
        plots1.append(plt1)
        names1.append(str(dkg) + " download")
        
        col1 = filtered.upload_bw_bytes.values

        assert len(x) == len(col1)
        plt1, = ax1.plot(x, col1, linestyle="--", marker="x")
        plots1.append(plt1)
        names1.append(str(dkg) + " upload")

    #plt.xticks(x, x, rotation=30)

    ax1.set_xticks(x)
    ax1.set_xticklabels(np.log2(x).astype(int), rotation=30)

    locmaj = plticker.LogLocator(base=10,numticks=12)
    ax1.yaxis.set_major_locator(locmaj)
    locmin = plticker.LogLocator(base=10.0,numticks=12)    #,subs=(0.2,0.4,0.6,0.8)
    ax1.yaxis.set_minor_locator(locmin)
    ax1.yaxis.set_minor_formatter(plticker.NullFormatter())

    ax1.legend(plots1,
               names1,
               loc='upper left')#, fontsize=fontsize

    plt.tight_layout()
    #date = time.strftime("%Y-%m-%d-%H-%M-%S")
    #out_png_file = 'append-only-proofs-' + date + '.png'
    plt.savefig(out_png_file, bbox_inches='tight')
    plt.close()

plotNumbers(csv_data)

#plt.xticks(fontsize=fontsize)
#plt.yticks(fontsize=fontsize)
plt.show()
