#!/usr/bin/python
import scipy
from numpy import *
import pylab as py
import scipy.linalg
import urllib, hashlib, os, random, re
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from skimage.filter import canny
import scipy.ndimage as ndimage

# E.g.
# g = get_gravatar_array("pnf@podsnap.com",sz=100,mask=((0,10),(80,90)),shrink=0.0,edge=(5,20,2,2))
# unis = get_unichars(100,100,snub=10)
# ya = anneal(unis,g, 1.0e7,3,1000000,50000,1)
 
i1 = 0
i2 = 4000
sz = 100
dim = 100

def get_unichars(sz=100,dim=100,snub=0,shifts=[]):
    files = os.listdir("chars")
    files = [(f,re.search("([\dabcdef]+)\.gif",f)) for f in files] #HEX.gif
    files = [(f,int(m.group(1),16)) for (f,m) in files if m]
    files = [(f,i) for (f,i) in files if i>0 and i<0x10000]  # max unicode in python
    C = [unichr(i) for (f,i) in files]
    F = [scipy.misc.imread("chars/" + f) for (f,i) in files]
    F = [scipy.misc.imresize(f,(sz,sz)) for f in F]
    for (dx,dy) in shifts:
        F.extend([shift(f,sz,dx,dy) for f in F])
        C.extend(C)
        #F.extend([shift(f,sz,nshift,0) for f in F])
        #C.extend(C)
        #F.extend([shift(f,sz,0,nshift) for f in F])
        #F.extend([shift(f,sz,0,-nshift) for f in F])
    F = array([f.flatten() for f in F])
    F = 255-255*F.transpose()     #n_pixels x n_chars
    (u,d,vh) = linalg.svd(F)
    # Columns of u are orthogonal basis of images (cols of v are onb of pixels...)
    # Projection of characters onto basis:
    u = u[:,snub:(snub+dim)]  # n_pixels x n_chars ==> n_pixels x n_dim
    ps = u.transpose().dot(F).transpose()  # n_chars x n_dims
    return (files,C,F,u,d,vh,ps)

# g3 = (ndimage.gaussian_filter(255*canny(g.reshape(100,100),3,10,2),sigma=1.5).flatten()>50)*255
# 4 =(ndimage.gaussian_filter(255*canny(g.reshape(100,100),3,25,2),sigma=2).flatten()>50)*255

def get_gravatar_array(email,sz=100,edge=(5,20,2,2),mask=((0,10),(80,90)),shrink=0.0):
    g = hashlib.md5(email.lower()).hexdigest() + ".jpg"
    fname = "/tmp/" + g
    url = "http://www.gravatar.com/avatar/" + g
    os.system("curl -s -o %s %s" % (fname,url))
    g = scipy.misc.imread(fname,flatten=True)[5:75,5:75]
    g = 255-scipy.misc.imresize(g,(sz,sz))
    if edge:
        (sigma,hi,low,sigma2) = edge
        g = 255*canny(g,sigma,hi,low)
        g = (ndimage.gaussian_filter(g,sigma=sigma2)>25)*255
    if mask:
        ((r1,c1),(r2,c2)) = mask
        g[r2:,:] = 0
        g[:r1,:] = 0
        g[:,:c1] = 0
        g[:,c2:] = 0
    if shrink>0.0:
        (nx,ny) = g.shape
        px = int(nx*shrink/2)
        py = int(ny*shrink/2)
        g = concatenate((zeros((py,nx),dtype=g.dtype),
                         g,
                         zeros((py,nx),dtype=g.dtype)), axis=0)
        g = concatenate((zeros((ny+2*py,px),dtype=g.dtype),
                         g,
                         zeros((ny+2*py,px),dtype=g.dtype)),axis=1)
        g = scipy.misc.imresize(g,(sz,sz))
    g = g.flatten()
    return g


def you1(unis,gravatar,n):
    (files,C,F,u,d,vh,ps) = unis
    pg = u.transpose().dot(gravatar)
    dist = [(ps[i]-pg).dot(ps[i]-pg) for i in range(len(ps))]
    ranks = sorted(range(len(dist)),key=dist.__getitem__)
    you = zeros(len(gravatar),dtype=uint8)
    state = ranks[:n]
    for i in range(0,n):
        you = you + F[:,state[i]]
    us = " ".join([C[i] for i in state])
    return (you,ranks[:n],us)


def anneal(unis,g,T=1.0e7,Nchar=4,Nsteps=1000000,status=50000,fig=1):
    (files,C,F,u,d,vh,ps) = unis
    sz = int(sqrt(len(g))+0.5)
    pg = u.transpose().dot(g)
    random.seed()
    Nunis = F.shape[1]
    # Start with the naive Nchar nearest characters:
    (y,state,us) = you1(unis,g,Nchar)
    state = array(state)
    aT = exp(-log(T)/float(Nsteps))
    nchange = 0
    ichanged = 0
    v = pg - ps[state,:].sum(axis=0)
    p = -v.dot(v)/T

    for i in range(Nsteps):
        j = random.randrange(Nchar)
        k = random.randrange(Nunis)
        kold = state[j]
        state[j] = k
        p = p * aT
        v = pg - ps[state,:].sum(axis=0)
        d = v.dot(v)
        #d = d*d
        p2 = -d/T
        r = log(random.random())
        if status and not i % status:
            us = " ".join([C[ii] for ii in state])
            print i,T,d,p,p2,(p-p2)/r,nchange,state,us
            if fig>=0:
                v = F[:,state].sum(axis=1)
                plt.figure(fig)
                plt.clf()
                plt.imshow(-((1.0*v+0.5*g)).reshape(sz,sz),cmap=cm.gray)
                plt.draw()
        if (p2>p or (p2-p)>r):
            p = p2
            nchange = nchange + 1
            ichanged = i
        else:
            state[j] = kold
        if (i-ichanged)>(Nsteps/10):
            print "Stopping",i,ichanged
            break
        T = T*aT
    v = zeros(len(g),dtype=uint8)
    for i in state:
        v = v + F[:,i]
    us = " ".join([C[i] for i in state])
    return (v,us,state)
