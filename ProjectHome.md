### It was good here, but Google Code is shutting down ###
### From 24 August 2015 ###
# EnKF-C is hosted at [GitHub](https://github.com/sakov/enkf-c) #

---

**EnKF-C** provides a light-weight generic framework for off-line data assimilation (DA) into large-scale layered geophysical models with the ensemble Kalman filter (EnKF). Here “light-weight” has higher priority than “generic”; that is, the code is not designed to cover every virtual possibility for the sake of it, but rather to be expandable in practical (from the author’s point of view) situations. Following are its other main features:

- coded in C for GNU/Linux platform;

- can conduct DA either in EnKF or ensemble optimal interpolation (EnOI) mode;

- permits multiple model grids;

- can handle rectangular or curvilinear horizontal grids, z or sigma vertical layers.

For more information see README and doc/enkf-userguide.pdf. (An older version of the user guide is also available from [arXiv](http://arxiv.org/abs/1410.1233).) Have a feel for how the code works by running the included example.

Checkout **EnKF-C** by running "`svn checkout http://enkf-c.googlecode.com/svn/enkf"`.