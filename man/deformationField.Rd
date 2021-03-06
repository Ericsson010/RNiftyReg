% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/transform.R
\name{deformationField}
\alias{deformationField}
\title{Calculate the deformation field for a transformation}
\usage{
deformationField(transform, jacobian = TRUE)
}
\arguments{
\item{transform}{A transform, possibly obtained from \code{\link{forward}}
or \code{\link{reverse}}.}

\item{jacobian}{A logical value: if \code{TRUE}, a Jacobian determinant map
is also calculated and returned in an attribute.}
}
\value{
An \code{"internalImage"} representing the deformation field. If
  requested, the Jacobian map is stored in an attribute, which can be
  extracted using the \code{\link{jacobian}} accessor function.
}
\description{
This function is used to calculate the deformation field corresponding to a
specified linear or nonlinear transformation. The deformation field gives
the location in source image space corresponding to the centre of each voxel
in target space. It is used as a common form for linear and nonlinear
transformations, and allows them to be visualised.
}
\author{
Jon Clayden <code@clayden.org>
}
\seealso{
\code{\link{niftyreg.linear}}, \code{\link{niftyreg.nonlinear}}
}

