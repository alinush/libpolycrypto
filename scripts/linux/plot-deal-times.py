#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib.ticker as plticker
import pandas
import sys
import time
import numpy as np

if len(sys.argv) < 5:
    print("Usage:", sys.argv[0], "<output-file-suffix> <min-N> <max-N> <csv-file> [<csv-file>] ...")
    sys.exit(0)

del sys.argv[0]

png_file_suffix = sys.argv[0]
del sys.argv[0]

minN = int(sys.argv[0])
del sys.argv[0]

maxN = int(sys.argv[0])
del sys.argv[0]

data_files = [f for f in sys.argv]

print("Reading CSV files:", data_files, "...")

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

print("min N:", minN)
print("max N:", maxN)

#csv_data = csv_data[csv_data.dkg != 'Feldman']
csv_data.vss.replace('amt', 'AMT DKG/VSS', inplace=True)
csv_data.vss.replace('kate', 'eVSS / eJF-DKG', inplace=True)
csv_data.vss.replace('feld', 'Feldman DKG/VSS', inplace=True)
csv_data = csv_data[csv_data.n >= minN]
csv_data = csv_data[csv_data.n <= maxN]

# we recompute these columns with more precision, even though the CSV file already has them
csv_data['avg_deal_sec']        = csv_data.avg_deal_usec        / (1000*1000)

print(csv_data.to_string())  # print all data

#SMALL_SIZE = 10
MEDIUM_SIZE = 20
BIG_SIZE = 24
BIGGER_SIZE = 28
BIGGEST_SIZE = 32

plt.rc('font',   size=      BIGGER_SIZE)    # controls default text sizes
#plt.rc('axes',   titlesize= BIGGER_SIZE)    # fontsize of the axes title
#plt.rc('axes',   labelsize= MEDIUM_SIZE)    # fontsize of the x and y labels
plt.rc('xtick',  labelsize= BIG_SIZE)
plt.rc('ytick',  labelsize= BIG_SIZE)
plt.rc('legend', fontsize=  BIGGER_SIZE)
#plt.rc('figure', titlesize=BIGGER_SIZE)     # fontsize of the figure title


def plotNumbers(data, colNames, legendAppendix, png_name, timeUnit='seconds', title=None):
    x = data.t.unique()    #.unique() # x-axis is the number of total players n

    #logBase = 10
    logBase = 2
    
    #print "Plotting with log base", logBase

    # adjust the size of the plot: (20, 10) is bigger than (10, 5) in the
    # sense that fonts will look smaller on (20, 10)
    fig, ax1 = plt.subplots(figsize=(12,7.5))
    ax1.set_xscale("log", basex=logBase)
    ax1.set_yscale("log")
    #ax1.set_xlabel("Total # of players n")
    #ax1.set_ylabel("Time (in " + timeUnit + ")")   #, fontsize=fontsize)
    ax1.grid(True)

    plots = []
    names = []

    #plt.xticks(x, x, rotation=30)
    schemes = data.vss.unique()
    for scheme in schemes:
        for i in range(len(colNames)):
            colName = colNames[i]
            appendix = legendAppendix[i]

            print("Plotting column", colName, "for:", scheme)

            filtered = data[data.vss == scheme]
            col1 = filtered[colName].values

            if len(x) > len(col1):
                print("WARNING: Not enough y-values on column", colName)
                print("Expected", len(x), "got", len(col1))

            linestyle="-"
            marker="o"

            plt1, = ax1.plot(x[:len(col1)], col1, linestyle=linestyle, linewidth=4, marker=marker, markersize=10)

            plots.append(plt1)
            names.append(scheme + appendix)

    #plt.xticks(x, x, rotation=30)

    #print "X:", x
    #print "len(x):", len(x)

    if title != None:
        ax1.set_title(title)
    ax1.set_xticks(x)
    ax1.set_xticklabels(np.log2(x).astype(int), rotation=30)

    locmaj = plticker.LogLocator(base=10,numticks=12)
    ax1.yaxis.set_major_locator(locmaj)
    locmin = plticker.LogLocator(base=10.0,numticks=12)    #,subs=(0.2,0.4,0.6,0.8)
    ax1.yaxis.set_minor_locator(locmin)
    ax1.yaxis.set_minor_formatter(plticker.NullFormatter())

    ax1.legend(plots,
               names,
               loc='upper left')#, fontsize=fontsize

    plt.tight_layout()

    out_file = png_name + png_file_suffix + ".png"
    print("Saving graph to", out_file)

    #date = time.strftime("%Y-%m-%d-%H-%M-%S")
    plt.savefig(out_file, bbox_inches='tight')
    plt.close()

plotNumbers(csv_data,
    ['avg_deal_sec'],
    [''],
    'all-deal-times')    # NOTE: we converted usecs to secs above

#plt.xticks(fontsize=fontsize)
#plt.yticks(fontsize=fontsize)
plt.show()
