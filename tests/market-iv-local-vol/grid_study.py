"""Independent interpolation of actual grids, with certified admission."""

from pde import GridCoefficients


class CertifiedGrid(GridCoefficients):
    @property
    def flat(self):
        return self.continuous.flat
