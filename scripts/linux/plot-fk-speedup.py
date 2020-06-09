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

#print(csv_data.columns)
#print(csv_data['dictSize'].values)
print(csv_data.dtypes)
#print(csv_data.n)

if minN == 0:
    minN = csv_data.n.unique().min();
if maxN == 0:
    maxN = csv_data.n.unique().max();

print("min N:", minN)
print("max N:", maxN)
print("has logscale:", has_logscale)

csv_data = csv_data[csv_data.n >= minN]
csv_data = csv_data[csv_data.n <= maxN]

#print(csv_data.to_string())

if 'vss' in csv_data:
    print("VSS .csv file detected!")
    proto_type = 'vss'
    suffix = 'VSS'
    verify_usec_cols = ['avg_verify_usec']

    csv_data.vss.replace('kate', 'eVSS', inplace=True)
    csv_data.vss.replace('feld', 'Feldman VSS', inplace=True)
    csv_data['avg_e2e_bc_usec']      = (csv_data.avg_deal_usec    +
                                       csv_data.avg_verify_usec  +
                                       csv_data.avg_reconstr_bc_usec)
    csv_data['avg_e2e_wc_usec']      = (csv_data.avg_deal_usec    +
                                       csv_data.avg_verify_usec  +
                                       csv_data.avg_reconstr_wc_usec)

if 'dkg' in csv_data:
    print("DKG .csv file detected!")
    proto_type = 'dkg'
    suffix = 'DKG'
    verify_usec_cols = ['avg_verify_best_case_usec', 'avg_verify_worst_case_usec']

    csv_data.dkg.replace('kate', 'eJF-DKG', inplace=True)
    csv_data.dkg.replace('feld', 'JF-DKG', inplace=True)
    csv_data['avg_e2e_bc_usec']      = (csv_data.avg_deal_usec    +
                                       csv_data.avg_verify_best_case_usec  +
                                       csv_data.avg_reconstr_bc_usec)
    csv_data['avg_e2e_wc_usec']      = (csv_data.avg_deal_usec    +
                                       csv_data.avg_verify_worst_case_usec  +
                                       csv_data.avg_reconstr_wc_usec)

csv_data[proto_type].replace('amt', 'AMT ' + suffix, inplace=True)
csv_data[proto_type].replace('fk', 'FK ' + suffix, inplace=True)

print_cols = ['t','n', proto_type,'avg_deal_usec', 'avg_reconstr_bc_usec', 'avg_reconstr_wc_usec', 'avg_e2e_bc_usec', 'avg_e2e_wc_usec']
print_cols.extend(verify_usec_cols)


#
# Compute speed-ups for FK versus AMT
# 
colnames = ['deal', 'reconstr_bc', 'reconstr_wc', 'e2e_bc', 'e2e_wc']
if proto_type == 'vss':
    colnames.append('verify')
elif proto_type == 'dkg':
    colnames.extend(['verify_best_case', 'verify_worst_case'])
else:
    print("ERROR: Unexpected protocol type: " + proto_type)
    sys.exit(1)
    
fk_speedup_cols = dict()
for name in colnames:
    fk_speedup_cols[name] = csv_data[csv_data[proto_type] == 'AMT ' + suffix]['avg_' + name + '_usec'].values / csv_data[csv_data[proto_type] == 'FK ' + suffix]['avg_' + name + '_usec'].values.astype(float)
    fk_speedup_cols[name] = fk_speedup_cols[name].round(decimals=2)

cols = [
    pandas.DataFrame(csv_data.t.unique(), columns=["t"]),
    pandas.DataFrame(csv_data.n.unique(), columns=["n"]),
    pandas.DataFrame((1.0 / fk_speedup_cols['deal']).round(decimals=2), columns=["amt_deal_speedup"]),
]

for name in colnames:
    cols.append(pandas.DataFrame(fk_speedup_cols[name], columns=[name]))

colname_to_friendly = {
    'deal'              : 'FK dealing speedup', 
    'verify'            : 'FK verify speedup', 
    'verify_best_case'  : 'FK verify (BC) speedup', 
    'verify_worst_case' : 'FK verify (WC) speedup', 
    'reconstr_bc'       : 'FK reconstr. (BC) speedup', 
    'reconstr_wc'       : 'FK reconstr. (WC) speedup', 
    'e2e_bc'            : 'FK e2e (BC) speedup', 
    'e2e_wc'            : 'FK e2e (WC) speedup',
}

fk_speedup = pandas.concat(cols, axis=1)

print(fk_speedup.to_string())

def plotNumbers(data, has_logscale, colNames, legendAppendix, png_name):
    x = data.t.unique()    #.unique() # x-axis is the number of total players n
    #print("x: " + str(x))
    
    #
    # NOTE: Using hex codes from here: https://matplotlib.org/devdocs/api/colors_api.html#module-matplotlib.colors
    #
    # '#d62728', # red
    # '#1f77b4', # blue
    # '#ff7f0e', # orange
    # '#2ca02c', # green
    colors = {
        "deal"              : '#d62728', # red
        "verify"            : '#1f77b4', # blue
        "verify_best_case"  : '#1f77b4', # blue
        "verify_worst_case" : '#1f77b4', # blue
        "reconstr_bc"       : '#ff7f0e', # orange
        "reconstr_wc"       : '#ff7f0e', # orange
        "e2e_bc"            : '#2ca02c', # green
        "e2e_wc"            : '#2ca02c', # green
    }

    #logBase = 10
    logBase = 2
    
    #print "Plotting with log base", logBase

    # adjust the size of the plot: (20, 10) is bigger than (10, 5) in the
    # sense that fonts will look smaller on (20, 10)
    fig, ax1 = plt.subplots(figsize=(12,7.5))
    if has_logscale:
        ax1.set_xscale("log", basex=logBase)
    #ax1.set_yscale("log")

    if has_logscale:
        ax1.set_xlabel("Log2 of threshold t (n = 2t-1)")
    else:
        ax1.set_xlabel("Threshold t (n = 2t-1)")
    ax1.set_ylabel("Speed up over AMT " + suffix)   #, fontsize=fontsize)
    ax1.grid(True)

    plots = []
    names = []

    #plt.xticks(x, x, rotation=30)
    for colName in colNames:
        appendix = legendAppendix[colName]

        print("Plotting FK speedup column: ", colName)

        col1 = data[colName].values

        if len(x) > len(col1):
            print("WARNING: Not enough y-values on column", colName)
            print("Expected", len(x), "got", len(col1))

        if '(BC)' in appendix:
            marker='x'
            linestyle='--'
        else:
            linestyle="-"
            marker="o"

        #print(colName + " values: " + str(col1))
        plt1, = ax1.plot(x[:len(col1)], col1, linestyle=linestyle, marker=marker, color=colors[colName])

        plots.append(plt1)
        names.append(appendix)

    #plt.xticks(x, x, rotation=30)

    #if title != None:
    #    ax1.set_title(title)
    ax1.set_xticks(x)
    ax1.set_yticks([0,1,2,3,4,5,6,7,8,9,10])  # TODO: get min/max speedup from loop above
    gridlines = ax1.yaxis.get_gridlines()
    gridlines[1].set_color("black")
    gridlines[1].set_linewidth(2)
    gridlines[1].set_linestyle('--')
    ax1.set_xticklabels(np.log2(x).astype(int), rotation=30)

    ax1.legend(plots,
               names,
               loc='upper left')#, fontsize=fontsize

    plt.tight_layout()

    out_file = png_name + png_file_suffix + ".png"
    print("Saving graph to", out_file)

    #date = time.strftime("%Y-%m-%d-%H-%M-%S")
    plt.savefig(out_file, bbox_inches='tight')
    plt.close()

SMALL_SIZE = 10
MEDIUM_SIZE = 20
BIG_SIZE = 24
BIGGER_SIZE = 28
BIGGEST_SIZE = 32

plt.rc('font',   size=BIGGER_SIZE)    # controls default text sizes
plt.rc('lines', linewidth=3, markersize=8, markeredgewidth=3)   # width of graph line & size of marker on graph line for data point
#plt.rc('axes',   titlesize= 10)    # fontsize of the axes title
plt.rc('axes',   labelsize= MEDIUM_SIZE)    # fontsize of the x and y labels set using set_xlabel and set_ylabel
plt.rc('xtick',  labelsize= MEDIUM_SIZE)
plt.rc('ytick',  labelsize= MEDIUM_SIZE)
#plt.rc('figure', titlesize=BIGGER_SIZE)     # fontsize of the figure title
plt.rc('legend', fontsize=20)

plotNumbers(fk_speedup, 
    has_logscale,
    colnames,
    colname_to_friendly,
    'fk-' + proto_type + '-speedup')

#plt.xticks(fontsize=fontsize)
#plt.yticks(fontsize=fontsize)
plt.show()
