#!/usr/bin/python
import scipy
from numpy import *
import pylab as py
import scipy.linalg
import urllib, hashlib, os, random
import matplotlib.pyplot as plt
 
i1 = 0
i2 = 4000
sz = 20

def get_unichars(i1,i2,sz,dim):
    # Columns of F will be candidates
    F = [scipy.misc.imread("output/uni%05d.png" % i) for i in range(i1,i2+1)]
    F = [scipy.misc.imresize(i,(sz,sz)) for i in F]
    F = array([i.flatten() for i in F])
    F = 255-F.transpose()
    (u,d,vh) = linalg.svd(F)
    # Columns of u are orthogonal basis of images (cols of v are onb of pixels...)
    # Projection of characters onto basis:
    u = u[:,:dim]
    ps = u.transpose().dot(F).transpose()
    return (F,u,d,vh,ps)

def get_gravatar_array(email,sz):
    gravatar = hashlib.md5(email.lower()).hexdigest() + ".jpg"
    fname = "/tmp/" + gravatar
    url = "http://www.gravatar.com/avatar/" + gravatar
    os.system("curl -s -o %s %s" % (fname,url))
    gravatar = scipy.misc.imread(fname,flatten=True)[5:75,5:75]
    gravatar = 255-scipy.misc.imresize(gravatar,(sz,sz)).flatten()
    return gravatar


#unis = (F,u,d,vh,ps) = get_unichars(i2,sz)

def you1(unis,gravatar,n):
    (F,u,d,vh,ps) = unis
    pg = u.transpose().dot(gravatar)
    dist = [(ps[i]-pg).dot(ps[i]-pg) for i in range(i1,i2+1)]
    ranks = sorted(range(len(dist)),key=dist.__getitem__)
    you = zeros(len(gravatar),dtype=uint8)
    for i in range(0,n):
        you = you + F[:,ranks[i]]
    return (you,ranks[:n])

def find_closest(v,vs):
    dmin = 0
    ic = 0
    for i in range(len(vs)):
        d = sqrt((vs[i]-v).dot(vs[i]-v))
        if i==0 or d<dmin:
            dmin = d
            ic = i
    print ic,d
    return ic

# Subtract last 
def you2(unis,gravatar,n):
    (F,u,d,vh,ps) = unis
    pg = u.transpose().dot(gravatar)
    you = zeros(len(gravatar),dtype=uint8)
    for i in range(n):
        ic = find_closest(pg,ps)
        print ic
        pg = pg - ps[ic]
        you = you + F[:,ic]
    return you

def find_closish(v,vs,prev,eps):
    dmin = 0
    ic = 0
    for i in range(len(vs)):
        d = sqrt((vs[i]-v).dot(vs[i]-v))
        ok = True
        if i==0 or d<dmin:
            for j in prev:
                dp = sqrt((vs[i]-vs[j]).dot(vs[i]-vs[j]))
                if dp<eps:
                    ok = False
                    continue
            if ok:
                dmin = d
                ic = i
    print dmin,ic
    return ic

# Ignore anything too close to what we've found so far.
def you3(unis,gravatar,n):
    (F,u,d,vh,ps) = unis
    pg = u.transpose().dot(gravatar)
    ret = []
    you = zeros(len(gravatar),dtype=uint8)
    for i in range(n):
        ic = find_closish(pg,ps,ret,100.0)
        ret.append(ic)
        you = you + F[:,ic]
    return (you,ret)

# Simulated annealing.
# probability will be exp(sum(d^2)/T)

# actually log of prob
def prob(ps,pg,dims,state,T):
    d = 0
    v = zeros(dims)
    for i in state:
        v = v + ps[i][:dims]
    v = pg[:dims] - v/len(state)
    return -v.dot(v)/T  # low is bad!
    

def anneal(unis,g,dims,T,Nchar,Nsteps,status=0,fig=-1):
    (F,u,d,vh,ps) = unis
    sz = int(sqrt(len(g))+0.5)
    pg = u.transpose().dot(g)
    random.seed()
    Nunis = len(F)
    #state = [random.randrange(Nunis) for i in range(Nchar)]
    (y,state) = you1(unis,g,Nchar)
    p = prob(ps,pg,dims,state,T)
    # select T so that current probability is 0.9
    # T = p/log(0.5)
    dT = T/float(Nsteps)
    for i in range(Nsteps):
        state2 = list(state)
        state2[random.randrange(Nchar)] = random.randrange(Nunis);
        p2 = prob(ps,pg,dims,state2,T)
        r = log(random.random())
        if status and not i % status:
            print i,T,p,p2,r,(p2-p)>r,exp(p2-p),exp(r),state,state2
            if fig>=0:
                v = zeros(len(g))
                for i in state:
                    v = v + F[:,i]
                v = v/len(state)
                plt.figure(fig)
                plt.clf()
                plt.imshow(v.reshape(sz,sz))
                plt.draw()
        if p2>p or (p2-p)>r:
            p = p2
            state = state2
        T = T - dT
    v = zeros(len(g))
    for i in state:
        v = v + F[:,i]
    v = v/len(state)
    us = repr("".join([unichr(i) for i in state])).decode("unicode-escape")
    return (v,us,state)
