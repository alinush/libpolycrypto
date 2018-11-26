#!/usr/bin/env python2.7

from cycler import cycler
import matplotlib.pyplot as plt
import matplotlib.ticker as plticker
import pandas
import sys
import time
import numpy as np

setTitle=False

if len(sys.argv) < 5:
    print "Usage:", sys.argv[0], "<output-file-prefix> <min-N> <max-N> <csv-file> [<csv-file>] ..."
    sys.exit(0)

del sys.argv[0]

out_png_file = sys.argv[0]
del sys.argv[0]

minN = int(sys.argv[0])
del sys.argv[0]

maxN = int(sys.argv[0])
del sys.argv[0]

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

#csv_data = csv_data[csv_data.dkg != 'Feldman']
csv_data.dkg.replace('feld', 'JF-DKG', inplace=True)
csv_data.dkg.replace('kate', 'eJF-DKG', inplace=True)
csv_data.dkg.replace('amt', 'AMT DKG', inplace=True)
csv_data.dkg.replace('old-feld', 'Old JF-DKG', inplace=True)
csv_data.dkg.replace('old-kate', 'Old eJF-DKG', inplace=True)
csv_data.dkg.replace('old-amt', 'Old AMT DKG', inplace=True)
csv_data = csv_data[csv_data.n >= minN]
csv_data = csv_data[csv_data.n <= maxN]


# In the best-case, avg_verify_bc_usec measures the time to verify {\sum_j p_j}(i) against \sum_j p_j, simulating aggregating multiple proofs and their commitments and then verifying a single agg. proof
csv_data['end_to_end_bc_usec'] = csv_data.avg_deal_usec + csv_data.avg_verify_best_case_usec  + csv_data.avg_reconstr_bc_usec
csv_data['end_to_end_wc_usec'] = csv_data.avg_deal_usec + csv_data.avg_verify_worst_case_usec + csv_data.avg_reconstr_wc_usec

# microsecs to secs
csv_data.avg_deal_usec /= 1000*1000     
csv_data.avg_verify_best_case_usec /= 1000*1000
csv_data.avg_verify_worst_case_usec /= 1000*1000
csv_data.avg_reconstr_bc_usec /= 1000*1000
csv_data.avg_reconstr_wc_usec /= 1000*1000
csv_data.end_to_end_bc_usec /= 1000*1000
csv_data.end_to_end_wc_usec /= 1000*1000

print csv_data[['t','n','dkg','avg_deal_hum','avg_verify_bc_hum', 'avg_verify_wc_hum', 'avg_reconstr_bc_hum', 'avg_reconstr_wc_hum']].to_string()  # print all data

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

VERIFY_TIME_TYPE = 1
DEAL_TIME_TYPE = 2
DEALVERIFY_TIME_TYPE = 3
DEALVERIFYRECONSTRUCT_TIME_TYPE = 4

def plotNumbers(data, bestCaseColName, worstCaseColName, title, fileSuffix):
    x = data.t.unique()    #.unique() # x-axis is the number of total players n

    #logBase = 10
    logBase = 2
    #print "Plotting with log base", logBase

    cols = []
    legend = {}
    linestyle = {}
    marker = {}

    cols.append(bestCaseColName)
    noWorstCase = worstCaseColName is None
    if noWorstCase:
        legend[bestCaseColName] = ""
        linestyle[bestCaseColName] = "-"
        marker[bestCaseColName] = "o"
    else:
        cols.append(worstCaseColName)

        legend[bestCaseColName] = " (best case)"
        legend[worstCaseColName] = " (worst case)"
        linestyle[bestCaseColName] = "--"
        linestyle[worstCaseColName] = "-"
        marker[bestCaseColName] = "x"
        marker[worstCaseColName] = "o"

    fig, ax1 = plt.subplots(figsize=(12,7.5))
    ax1.set_xscale("log", basex=logBase)
    ax1.set_yscale("log")
    #ax1.set_xlabel("Total # of players n")
    #ax1.set_ylabel("Time (in seconds)")   #, fontsize=fontsize)

    #plt.xticks(x, x, rotation=30)
    dkgs = data.dkg.unique()
    print
    print "X:", x
    print "len(x):", len(x)
        
    plots = []
    names = []

    for dkg in dkgs:
        for colName in cols: 
            print "Plotting", dkg
            filtered = data[data.dkg == dkg]
            usec_col = filtered[colName].values

            if setTitle:
                ax1.set_title(title)

            ax1.grid(True)

            assert len(x) == len(usec_col)
            plt1, = ax1.plot(x, usec_col, linestyle=linestyle[colName], marker=marker[colName])
            plots.append(plt1)
            names.append(str(dkg) + legend[colName])
        
            print "Y:", usec_col

    #plt.xticks(x, x, rotation=30)

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

    out_file = out_png_file + fileSuffix + ".png"
    print "Saving graph to", out_file

    #date = time.strftime("%Y-%m-%d-%H-%M-%S")
    plt.savefig(out_file, bbox_inches='tight')
    plt.close()

def plotReconstr(data, bestCaseColName, worstCaseColName, title, fileSuffix):
    x = data.t.unique()    #.unique() # x-axis is the number of total players n

    #logBase = 10
    logBase = 2
    #print "Plotting with log base", logBase

    legend = {}
    linestyle = {}
    marker = {}

    noWorstCase = worstCaseColName is None
    if noWorstCase:
        legend[bestCaseColName] = ""
        linestyle[bestCaseColName] = "-"
        marker[bestCaseColName] = "o"
    else:
        legend[bestCaseColName] = " (best case)"
        legend[worstCaseColName] = " (worst case)"
        linestyle[bestCaseColName] = "--"
        linestyle[worstCaseColName] = "-"
        marker[bestCaseColName] = "x"
        marker[worstCaseColName] = "o"

    fig, ax1 = plt.subplots(figsize=(12,7.5))
    ax1.set_xscale("log", basex=logBase)
    ax1.set_yscale("log")
    #ax1.set_xlabel("Total # of players n")
    #ax1.set_ylabel("Time (in seconds)")   #, fontsize=fontsize)

    #plt.xticks(x, x, rotation=30)
    print
    print "X:", x
    print "len(x):", len(x)
        
    dkgs = data.dkg.unique()
    plots = []
    names = []

    for colName in [ worstCaseColName ]:
        for dkg in dkgs:
            print "Plotting", dkg
            filtered = data[data.dkg == dkg]
            usec_col = filtered[colName].values

            if setTitle:
                ax1.set_title(title)

            ax1.grid(True)

            assert len(x) == len(usec_col)
            plt1, = ax1.plot(x, usec_col, linestyle=linestyle[colName], marker=marker[colName])
            plots.append(plt1)
            names.append(str(dkg) + legend[colName])
        
            print "Y:", usec_col
    
    dkg = dkgs[0]
    colName = bestCaseColName
    print "Plotting", dkg
    filtered = data[data.dkg == dkg]
    usec_col = filtered[colName].values

    assert len(x) == len(usec_col)
    plt1, = ax1.plot(x, usec_col, linestyle=linestyle[colName], marker=marker[colName])
    plots.append(plt1)
    names.append("eJF & AMT DKG" + legend[colName])

    print "Y:", usec_col

    #plt.xticks(x, x, rotation=30)

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

    out_file = out_png_file + fileSuffix + ".png"
    print "Saving graph to", out_file

    #date = time.strftime("%Y-%m-%d-%H-%M-%S")
    plt.savefig(out_file, bbox_inches='tight')
    plt.close()

#plt.rcParams['axes.prop_cycle'] = cycler(color='bgrcmyk')
plt.rcParams['axes.prop_cycle'] = cycler(color=[
    '#1f77b4', # blue
    '#ff7f0e', # orange
    ])

plotNumbers(
    data=csv_data,
    bestCaseColName='avg_deal_usec',
    worstCaseColName=None,
    title='DKG per-player deal times',
    fileSuffix='-deal-times')

#plt.rcParams['axes.prop_cycle'] = cycler(color='bgrcmyk')
plt.rcParams['axes.prop_cycle'] = cycler(color=[
    '#1f77b4', # blue
    '#1f77b4', # blue
    '#ff7f0e', # orange
    '#ff7f0e', # orange
    ])

plt.rc('legend', fontsize=  26)
plotNumbers(
    data=csv_data,
    bestCaseColName='avg_verify_best_case_usec',
    worstCaseColName='avg_verify_worst_case_usec',
    title='DKG per-player verify times',
    fileSuffix='-verify-times')

#plt.rcParams['axes.prop_cycle'] = cycler(color='bgrcmyk')
plt.rcParams['axes.prop_cycle'] = cycler(color=[
    '#1f77b4', # blue
    '#ff7f0e', # orange
    '#ff7f0e', # orange
    ])

plt.rc('legend', fontsize=  BIG_SIZE)
plotReconstr(
    data=csv_data,
    bestCaseColName='avg_reconstr_bc_usec',
    worstCaseColName='avg_reconstr_wc_usec',
    title='DKG reconstruct times',
    fileSuffix='-reconstr-times')

#plt.rcParams['axes.prop_cycle'] = cycler(color='bgrcmyk')
plt.rcParams['axes.prop_cycle'] = cycler(color=[
    '#1f77b4', # blue
    '#1f77b4', # blue
    '#ff7f0e', # orange
    '#ff7f0e', # orange
    ])

plt.rc('legend', fontsize=  BIGGER_SIZE)
plotNumbers(
    data=csv_data,
    bestCaseColName='end_to_end_bc_usec',
    worstCaseColName='end_to_end_wc_usec',
    title='DKG per-player end-to-end times',
    fileSuffix='-e2e-times')

#plt.xticks(fontsize=fontsize)
#plt.yticks(fontsize=fontsize)
plt.show()
