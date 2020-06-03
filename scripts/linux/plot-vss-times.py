#!/usr/bin/env python3

from cycler import cycler
import matplotlib.pyplot as plt
import matplotlib.ticker as plticker
import pandas
import sys
import time
import numpy as np

if len(sys.argv) < 6:
    print("Usage:", sys.argv[0], "<output-file-suffix> <min-N> <max-N> <has-logscale> <csv-file> [<csv-file>] ...")
    sys.exit(0)

del sys.argv[0]

png_file_suffix = sys.argv[0]
del sys.argv[0]

minN = int(sys.argv[0])
del sys.argv[0]

maxN = int(sys.argv[0])
del sys.argv[0]

has_logscale = int(sys.argv[0]) != 0
del sys.argv[0]

data_files = [f for f in sys.argv]

print("Reading CSV files:", data_files, "...")

csv_data = pandas.concat((pandas.read_csv(f) for f in data_files))
csv_data.sort_values('t', inplace=True) # WARNING: Otherwise, plotting unsorted CSV file be messed up, with incorrect y-values for a x-coordinates

#print("Raw:")
#print(csv_data.to_string())
#print(csv_data.columns)
#print(csv_data['dictSize'].values)

#print("Averaged:")

if minN == 0:
    minN = csv_data.n.unique().min();
if maxN == 0:
    maxN = csv_data.n.unique().max();

print("min N:", minN)
print("max N:", maxN)
print("has logscale:", has_logscale)

if 'vss' in csv_data:
    print("VSS .csv file detected!")
    proto_type = 'vss'
    proto_type_caps = 'VSS'
    verify_ms_cols = ['avg_verify_ms']
    verify_ms_appendix = ['']

    csv_data.vss.replace('kate', 'eVSS', inplace=True)
    csv_data.vss.replace('feld', 'Feldman VSS', inplace=True)
    csv_data['avg_verify_ms']   = csv_data.avg_verify_usec / (1000)
    csv_data['avg_e2e_bc_usec'] = (csv_data.avg_deal_usec    +
                                  csv_data.avg_verify_usec  +
                                  csv_data.avg_reconstr_bc_usec)
    csv_data['avg_e2e_wc_usec'] = (csv_data.avg_deal_usec    +
                                  csv_data.avg_verify_usec  +
                                  csv_data.avg_reconstr_wc_usec)

if 'dkg' in csv_data:
    print("DKG .csv file detected!")
    proto_type = 'dkg'
    proto_type_caps = 'DKG'
    verify_ms_cols = ['avg_verify_bc_ms', 'avg_verify_wc_ms']
    verify_ms_appendix = [' best-case', ' worst-case']

    csv_data.dkg.replace('kate', 'eJF-DKG', inplace=True)
    csv_data.dkg.replace('feld', 'JF-DKG', inplace=True)
    csv_data['avg_verify_bc_ms'] = csv_data.avg_verify_best_case_usec / (1000)
    csv_data['avg_verify_wc_ms'] = csv_data.avg_verify_worst_case_usec / (1000)
    csv_data['avg_e2e_bc_usec']  = (csv_data.avg_deal_usec    +
                                   csv_data.avg_verify_best_case_usec  +
                                   csv_data.avg_reconstr_bc_usec)
    csv_data['avg_e2e_wc_usec']  = (csv_data.avg_deal_usec    +
                                   csv_data.avg_verify_worst_case_usec  +
                                   csv_data.avg_reconstr_wc_usec)

csv_data[proto_type].replace('amt', 'AMT ' + proto_type_caps, inplace=True)
csv_data[proto_type].replace('fk', 'FK ' + proto_type_caps, inplace=True)

print_cols = ['t','n', proto_type, 'avg_deal_usec', 'avg_reconstr_bc_usec', 'avg_reconstr_wc_usec', 'avg_e2e_bc_usec', 'avg_e2e_wc_usec']
print_cols.extend(verify_ms_cols)

csv_data['avg_deal_sec']        = csv_data.avg_deal_usec        / (1000*1000)
csv_data['avg_reconstr_bc_sec'] = csv_data.avg_reconstr_bc_usec / (1000*1000)
csv_data['avg_reconstr_wc_sec'] = csv_data.avg_reconstr_wc_usec / (1000*1000)
csv_data['avg_e2e_bc_sec']      = csv_data.avg_e2e_bc_usec / (1000*1000)
csv_data['avg_e2e_wc_sec']      = csv_data.avg_e2e_wc_usec / (1000*1000)

print(csv_data[print_cols].to_string())  # print(all data)

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

def plotNumbers(data, has_logscale, colNames, legendAppendix, png_name, timeUnit='seconds', title=None):
    x = data.t.unique()    #.unique() # x-axis is the number of total players n
    #print("x: " + str(x))
    
    # '#1f77b4', # blue
    # '#ff7f0e', # orange
    # '#2ca02c', # green
    # '#d62728', # red
    colors = {
        "eVSS" : "#1f77b4",
        "eJF-DKG" : "#1f77b4",

        "FK VSS & eVSS" : "#1f77b4",
        "eJF & FK DKG"  : "#1f77b4",

        "AMT VSS" : "#ff7f0e",
        "AMT DKG" : "#ff7f0e",

        "FK VSS" :'#2ca02c',
        "FK DKG" :'#2ca02c',

        "Feldman VSS" : '#d62728',
        "Feldman DKG" : '#d62728',
    }

    #logBase = 10
    logBase = 2
    
    #print("Plotting with log base", logBase)

    # adjust the size of the plot: (20, 10) is bigger than (10, 5) in the
    # sense that fonts will look smaller on (20, 10)
    fig, ax1 = plt.subplots(figsize=(12,7.5))
    if has_logscale:
        ax1.set_xscale("log", basex=logBase)

    # verification times are in 2 - 20 ms range, so log scale is bad
    if png_name != 'vss-verify-times' and has_logscale:
        print("Setting log y-scale")
        ax1.set_yscale("log")
    else:
        print("Not setting log y-scale")

    ax1.set_xlabel("Threshold t (where n = 2t-1)")
    ax1.set_ylabel("Time (in " + timeUnit + ")")   #, fontsize=fontsize)
    ax1.grid(True)

    plots = []
    names = []

    #plt.xticks(x, x, rotation=30)
    schemes = data[proto_type].unique()
    for scheme in schemes:
        for i in range(len(colNames)):
            colName = colNames[i]
            appendix = legendAppendix[i]

            print("Plotting column", colName, "for:", scheme)

            filtered = data[data[proto_type] == scheme]
            col1 = filtered[colName].values

            if len(x) > len(col1):
                print("WARNING: Not enough y-values on column", colName)
                print("Expected", len(x), "got", len(col1))

            if 'best-case' in appendix:
                marker='x'
                linestyle='--'
            else:
                linestyle="-"
                marker="o"

            #print(colName + " values: " + str(col1))
            plt1, = ax1.plot(x[:len(col1)], col1, linestyle=linestyle, marker=marker, color=colors[scheme])

            plots.append(plt1)
            names.append(scheme + appendix)

    #plt.xticks(x, x, rotation=30)

    #print("X:", x)
    #print("len(x):", len(x))

    if title != None:
        ax1.set_title(title)
    ax1.set_xticks(x)
    ax1.set_xticklabels(np.log2(x).astype(int), rotation=30)

    
    # verification times are in 2 - 20 ms range, so log scale is bad
    if png_name != 'vss-verify-times':
    #and png_name != 'dkg-verify-times':
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

#
# Deal times
# NOTE: Using hex codes from here: https://matplotlib.org/devdocs/api/colors_api.html#module-matplotlib.colors
#
plt.rc('legend', fontsize=  BIGGER_SIZE)
plotNumbers(csv_data, has_logscale,
    ['avg_deal_sec'],
    [''],
    proto_type + '-deal-times')    # NOTE: we converted usecs to secs above


#
# End-to-end times
#
plt.rc('legend', fontsize=24)
plotNumbers(csv_data, has_logscale,
    ['avg_e2e_bc_sec', 'avg_e2e_wc_sec'],
    [' best-case', ' worst-case'],
    proto_type + '-e2e-times')

plotNumbers(csv_data, has_logscale,
    ['avg_e2e_bc_sec'],
    [' best-case'],
    proto_type + '-e2e-bc-times')
plotNumbers(csv_data, has_logscale,
    ['avg_e2e_wc_sec'],
    [' worst-case'],
    proto_type + '-e2e-wc-times')

#
# Verification times
#

# NOTE: eVSS and FK have the same verification & reconstruction cost, since the proofs are the same
if 'vss' in csv_data and 'eVSS' in csv_data.vss.unique() and 'FK VSS' in csv_data.vss.unique():
    # remove FK numbers and just display eVSS numbers as "FK VSS & eVSS"
    csv_data.vss.replace('eVSS', 'FK VSS & eVSS', inplace=True)
    csv_data = csv_data[csv_data.vss != "FK VSS"]

if 'dkg' in csv_data and 'eJF-DKG' in csv_data.dkg.unique() and 'FK DKG' in csv_data.dkg.unique():
    # remove FK numbers and just display eJF-DKG numbers as "eJF & FK DKG"
    csv_data.dkg.replace('eJF-DKG', 'eJF & FK DKG', inplace=True)
    csv_data = csv_data[csv_data.dkg != "FK DKG"]

plt.rc('legend', fontsize=  BIGGER_SIZE)
plotNumbers(csv_data, has_logscale,
    verify_ms_cols,
    verify_ms_appendix,
    proto_type + '-verify-times',
    'milliseconds')

#
# Reconstruction times
#
plt.rc('legend', fontsize=23)
plotNumbers(csv_data, has_logscale,
    ['avg_reconstr_bc_sec', 'avg_reconstr_wc_sec'],
    [' best-case', ' worst-case'],
    proto_type + '-reconstr-times')

#plt.xticks(fontsize=fontsize)
#plt.yticks(fontsize=fontsize)
plt.show()
