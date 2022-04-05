# DAL - *D*erivatives *A*lgorithms *L*ib

<table>
<tr>
  <td>Build Status</td>
  <td>
    <a href="https://travis-ci.org/wegamekinglc/Derivatives-Algorithms-Lib">
    <img src="https://travis-ci.org/wegamekinglc/Derivatives-Algorithms-Lib.svg?branch=master" alt="travis build status" />
    </a>
  </td>
</tr>
<tr>
  <td>Coverage</td>
  <td><img src="https://coveralls.io/repos/wegamekinglc/Derivatives-Algorithms-Lib/badge.svg?branch=master" alt="coverage" /></td>
</tr>
</table>


## Introduction

This is a project inspired by following books:

* [*Derivatives Algorithms:  Volume 1: Bones* by Tom Hyer](https://github.com/TomHyer/DA_Bones_Mirror)
  
* [*Modern Computational Finance: AAD and Parallel Simulations* by Antoine Savine](https://github.com/asavine/CompFinance)

* [*Modern Computational Finance: Scripting for Derivatives and xVA* by Antoine Savine](https://github.com/asavine/Scripting)

At current stage, this codes repository is simply for prototyping and practice. Don't use them in serious situation.

> Some codes are directly copied from Tom and Antoine's books.

## Interface

> **NOTE**: This part is only in infancy and should evolve quickly.

We will give a public interface to show the functionality of this project.

### Excel

we have following data table

| **x** 	  | **y** 	  |
|------|------|
| 1 	  | 10 	 |
| 3 	  | 8 	  |
| 5 	  | 6 	  |
| 7 	  | 4 	  |
| 9 	  | 2 	  |

and we will use follow excel function to create a linear interpolator:

```excel
=INTERP1.NEW.LINEAR(E1,A2:A6,B2:B6)  # return a object string id, e.g. ~Interp1~my.interp~2F18E558
```

later we can use the interpolator:
```excel
=INTERP1.GET("~Interp1~my.interp~2F18E558", 6.5)  # will return 4.5
```


