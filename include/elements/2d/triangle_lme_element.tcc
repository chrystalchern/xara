//! Assign nodal connectivity property for LME elements
template <unsigned Tdim>
void mpm::TriangleLMEElement<Tdim>::initialise_lme_connectivity_properties(
    double beta, double radius, const unsigned anisotropy,
    const Eigen::MatrixXd& nodal_coordinates) {
  this->nconnectivity_ = nodal_coordinates.rows();
  this->nodal_coordinates_ = nodal_coordinates;
  this->beta_ = beta;
  this->anisotropy_ = anisotropy;
  this->support_radius_ = radius;
}

//! Return shape functions of a Quadrilateral LME Element at a given
//! local coordinate
template <unsigned Tdim>
inline Eigen::VectorXd mpm::TriangleLMEElement<Tdim>::shapefn(
    const Eigen::Matrix<double, Tdim, 1>& xi,
    Eigen::Matrix<double, Tdim, 1>& lambda,
    const Eigen::Matrix<double, Tdim, Tdim>& deformation_gradient) const {

  //! To store shape functions
  Eigen::VectorXd shapefn =
      Eigen::VectorXd::Constant(this->nconnectivity_, 1.0);

  //! Substitute the TriLME by the FEM shape funtion in the limit of the simplex
  if (this->nconnectivity_ == 3)
    return mpm::TriangleElement<Tdim, 3>::shapefn(xi, lambda,
                                                  deformation_gradient);

  try {
    //! Convert local coordinates to real coordinates
    Eigen::Matrix<double, Tdim, 1> pcoord;
    pcoord.setZero();
    auto local_shapefn = this->shapefn_local(xi, lambda, deformation_gradient);
    for (unsigned i = 0; i < local_shapefn.size(); ++i)
      pcoord.noalias() +=
          local_shapefn(i) * nodal_coordinates_.row(i).transpose();

    //! Create relative coordinate vector
    const auto rel_coordinates =
        (-nodal_coordinates_.transpose()).colwise() + pcoord;

    //! Create metric tensor
    Eigen::MatrixXd metric = Eigen::Matrix<double, Tdim, Tdim>::Identity();

    if (anisotropy_) {
      //! Anisotropic metric tensor
      const auto inverse_deformation_gradient = deformation_gradient.inverse();
      metric = inverse_deformation_gradient.transpose() *
               inverse_deformation_gradient;
    }

    //! Compute functional f in each connectivity
    Eigen::VectorXd f = Eigen::VectorXd::Constant(this->nconnectivity_, 0.0);
    double sum_exp_f = 0.;
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      const double distance = std::sqrt((rel_coordinates.col(n)).transpose() *
                                        (metric * rel_coordinates.col(n)));
      if (distance < this->support_radius_) {
        f(n) =
            -beta_ * distance * distance + lambda.dot(rel_coordinates.col(n));
        sum_exp_f += std::exp(f(n));
      }
    }

    //! Compute p in each connectivity
    Eigen::VectorXd p = Eigen::VectorXd::Constant(this->nconnectivity_, 0.0);
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      const double distance = std::sqrt((rel_coordinates.col(n)).transpose() *
                                        metric * rel_coordinates.col(n));
      if (distance < this->support_radius_) p(n) = std::exp(f(n)) / sum_exp_f;
    }

    //! Compute vector r
    VectorDim r = VectorDim::Zero();
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      r.noalias() += p(n) * (rel_coordinates.col(n));
    }

    //! Begin regularized Newton-Raphson iteration
    const double tolerance = 1.e-12;
    if (r.norm() > tolerance) {
      bool convergence = false;
      unsigned it = 1;
      const unsigned max_it = 10;
      while (!convergence) {
        //! Compute matrix J
        Eigen::MatrixXd J(Tdim, Tdim);
        J.setZero();
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          J.noalias() += p(n) * (rel_coordinates.col(n)) *
                         (rel_coordinates.col(n)).transpose();
        }
        J.noalias() += -r * r.transpose();

        //! Add preconditioner for J (Mathieu Foca, PhD Thesis)
        for (unsigned i = 0; i < Tdim; i++) J.diagonal()[i] += r.norm();

        //! Compute Delta lambda
        VectorDim dlambda = J.inverse() * (-r);
        lambda = lambda + dlambda;

        //! Reevaluate f, p, and r
        //! Compute functional f in each connectivity
        sum_exp_f = 0.;
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          const double distance =
              std::sqrt((rel_coordinates.col(n)).transpose() * metric *
                        rel_coordinates.col(n));
          if (distance < this->support_radius_) {
            f(n) = -beta_ * distance * distance +
                   lambda.dot(rel_coordinates.col(n));
            sum_exp_f += std::exp(f(n));
          }
        }

        //! Compute p in each connectivity
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          const double distance =
              std::sqrt((rel_coordinates.col(n)).transpose() * metric *
                        rel_coordinates.col(n));
          if (distance < this->support_radius_)
            p(n) = std::exp(f(n)) / sum_exp_f;
        }

        //! Compute vector r
        r.setZero();
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          r.noalias() += p(n) * (rel_coordinates.col(n));
        }

        //! Check convergence
        if (r.norm() < tolerance) {
          convergence = true;
        } else if (it == max_it) {
          console_->warn(
              "Max number of iteration for shapefn computation is "
              "reached!");

          //! Check condition number
          Eigen::JacobiSVD<Eigen::MatrixXd> svd(J);
          const double rcond =
              svd.singularValues()(svd.singularValues().size() - 1) /
              svd.singularValues()(0);
          if (rcond < 1E-8) console_->warn("The Hessian matrix is singular!");

          convergence = true;
        }
        it++;
      }
    }

    // Assign shape function
    shapefn = p;

  } catch (std::exception& exception) {
    console_->error("{} #{}: {}\n", __FILE__, __LINE__, exception.what());
    return shapefn;
  }
  return shapefn;
}

//! Return gradient of shape functions of a Quadrilateral LME Element at a
//! given local coordinate
template <unsigned Tdim>
inline Eigen::MatrixXd mpm::TriangleLMEElement<Tdim>::grad_shapefn(
    const Eigen::Matrix<double, Tdim, 1>& xi,
    Eigen::Matrix<double, Tdim, 1>& lambda,
    const Eigen::Matrix<double, Tdim, Tdim>& deformation_gradient) const {

  //! To store grad shape functions
  Eigen::MatrixXd grad_shapefn(this->nconnectivity_, Tdim);

  if (this->nconnectivity_ == 3)
    return mpm::TriangleElement<Tdim, 3>::grad_shapefn(xi, lambda,
                                                       deformation_gradient);

  try {
    //! Convert local coordinates to real coordinates
    Eigen::Matrix<double, Tdim, 1> pcoord;
    pcoord.setZero();
    auto local_shapefn = this->shapefn_local(xi, lambda, deformation_gradient);
    for (unsigned i = 0; i < local_shapefn.size(); ++i)
      pcoord.noalias() +=
          local_shapefn(i) * nodal_coordinates_.row(i).transpose();

    //! Create relative coordinate vector
    const auto rel_coordinates =
        (-nodal_coordinates_.transpose()).colwise() + pcoord;

    //! Create metric tensor
    Eigen::MatrixXd metric = Eigen::Matrix<double, Tdim, Tdim>::Identity();

    if (anisotropy_) {
      //! Anisotropic metric tensor
      const auto inverse_deformation_gradient = deformation_gradient.inverse();
      metric = inverse_deformation_gradient.transpose() *
               inverse_deformation_gradient;
    }

    //! Compute functional f in each connectivity
    Eigen::VectorXd f = Eigen::VectorXd::Constant(this->nconnectivity_, 0.0);
    double sum_exp_f = 0.;
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      const double distance = std::sqrt((rel_coordinates.col(n)).transpose() *
                                        metric * rel_coordinates.col(n));
      if (distance < this->support_radius_) {
        f(n) =
            -beta_ * distance * distance + lambda.dot(rel_coordinates.col(n));
        sum_exp_f += std::exp(f(n));
      }
    }

    //! Compute p in each connectivity
    Eigen::VectorXd p = Eigen::VectorXd::Constant(this->nconnectivity_, 0.0);
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      const double distance = std::sqrt((rel_coordinates.col(n)).transpose() *
                                        metric * rel_coordinates.col(n));
      if (distance < this->support_radius_) p(n) = std::exp(f(n)) / sum_exp_f;
    }

    //! Compute vector r
    VectorDim r = VectorDim::Zero();
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      r.noalias() += p(n) * (rel_coordinates.col(n));
    }

    //! Compute matrix J
    Eigen::MatrixXd J(Tdim, Tdim);
    J.setZero();
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      J.noalias() += p(n) * (rel_coordinates.col(n)) *
                     (rel_coordinates.col(n)).transpose();
    }
    J.noalias() += -r * r.transpose();

    //! Add preconditioner for J (Mathieu Foca, PhD Thesis)
    for (unsigned i = 0; i < Tdim; i++) J.diagonal()[i] += r.norm();

    //! Begin Newton-Raphson iteration
    const double tolerance = 1.e-12;
    if (r.norm() > tolerance) {
      bool convergence = false;
      unsigned it = 1;
      unsigned max_it = 10;
      while (!convergence) {
        //! Compute Delta lambda
        VectorDim dlambda = J.inverse() * (-r);
        lambda = lambda + dlambda;

        //! Reevaluate f, p, and r
        //! Compute functional f in each connectivity
        sum_exp_f = 0.;
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          const double distance =
              std::sqrt((rel_coordinates.col(n)).transpose() * metric *
                        rel_coordinates.col(n));
          if (distance < this->support_radius_) {
            f(n) = -beta_ * std::pow(distance, 2) +
                   lambda.dot(rel_coordinates.col(n));
            sum_exp_f += std::exp(f(n));
          }
        }

        //! Compute p in each connectivity
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          const double distance =
              std::sqrt((rel_coordinates.col(n)).transpose() * metric *
                        rel_coordinates.col(n));
          if (distance < this->support_radius_)
            p(n) = std::exp(f(n)) / sum_exp_f;
        }

        //! Compute vector r
        r.setZero();
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          r.noalias() += p(n) * (rel_coordinates.col(n));
        }

        //! Compute matrix J
        J.setZero();
        for (unsigned n = 0; n < this->nconnectivity_; ++n) {
          J.noalias() += p(n) * (rel_coordinates.col(n)) *
                         (rel_coordinates.col(n)).transpose();
        }
        J.noalias() += -r * r.transpose();

        //! Check convergence
        if (r.norm() <= tolerance || it == max_it) convergence = true;
        it++;
      }
    }

    // Compute shape function gradient
    for (unsigned n = 0; n < this->nconnectivity_; ++n) {
      const VectorDim grad_p = -p(n) * J.inverse() * (rel_coordinates.col(n));
      grad_shapefn.row(n) = grad_p.transpose();
    }

  } catch (std::exception& exception) {
    console_->error("{} #{}: {}\n", __FILE__, __LINE__, exception.what());
    return grad_shapefn;
  }
  return grad_shapefn;
}

//! Compute dn_dx
template <unsigned Tdim>
inline Eigen::MatrixXd mpm::TriangleLMEElement<Tdim>::dn_dx(
    const VectorDim& xi, const Eigen::MatrixXd& nodal_coordinates,
    VectorDim& lambda, const MatrixDim& deformation_gradient) const {
  // Get gradient shape functions
  return this->grad_shapefn(xi, lambda, deformation_gradient);
}

//! Return the B-matrix of a Quadrilateral Element at a given local
//! coordinate for a real cell
template <unsigned Tdim>
inline std::vector<Eigen::MatrixXd> mpm::TriangleLMEElement<Tdim>::bmatrix(
    const VectorDim& xi, const Eigen::MatrixXd& nodal_coordinates,
    VectorDim& lambda, const MatrixDim& deformation_gradient) const {

  // Get gradient shape functions
  Eigen::MatrixXd grad_sf =
      this->grad_shapefn(xi, lambda, deformation_gradient);

  // B-Matrix
  std::vector<Eigen::MatrixXd> bmatrix;
  bmatrix.reserve(this->nconnectivity_);

  try {
    // Check if matrices dimensions are correct
    if ((grad_sf.rows() != nodal_coordinates.rows()) ||
        (xi.rows() != nodal_coordinates.cols()))
      throw std::runtime_error(
          "BMatrix - Jacobian calculation: Incorrect dimension of xi and "
          "nodal_coordinates");
  } catch (std::exception& exception) {
    console_->error("{} #{}: {}\n", __FILE__, __LINE__, exception.what());
    return bmatrix;
  }

  // Jacobian dx_i/dxi_j
  Eigen::Matrix<double, Tdim, Tdim> jacobian =
      (grad_sf.transpose() * nodal_coordinates);

  // Gradient shapefn of the cell
  // dN/dx = [J]^-1 * dN/dxi
  Eigen::MatrixXd grad_shapefn = grad_sf * (jacobian.inverse()).transpose();

  for (unsigned i = 0; i < this->nconnectivity_; ++i) {
    Eigen::Matrix<double, 3, Tdim> bi;
    // clang-format off
          bi(0, 0) = grad_shapefn(i, 0); bi(0, 1) = 0.;
          bi(1, 0) = 0.;                 bi(1, 1) = grad_shapefn(i, 1);
          bi(2, 0) = grad_shapefn(i, 1); bi(2, 1) = grad_shapefn(i, 0);
          bmatrix.push_back(bi);
    // clang-format on
  }
  return bmatrix;
}

//! Return local shape functions of a LME Triangle Element at a given
//! local coordinate, with particle size and deformation gradient
template <unsigned Tdim>
inline Eigen::VectorXd mpm::TriangleLMEElement<Tdim>::shapefn_local(
    const VectorDim& xi, VectorDim& lambda,
    const MatrixDim& deformation_gradient) const {
  return mpm::TriangleElement<Tdim, 3>::shapefn(xi, lambda,
                                                deformation_gradient);
}

//! Compute Jacobian with particle size and deformation gradient
template <unsigned Tdim>
inline Eigen::Matrix<double, Tdim, Tdim>
    mpm::TriangleLMEElement<Tdim>::jacobian(
        const VectorDim& xi, const Eigen::MatrixXd& nodal_coordinates,
        VectorDim& lambda, const MatrixDim& deformation_gradient) const {

  // Get gradient shape functions
  const Eigen::MatrixXd grad_shapefn =
      this->grad_shapefn(xi, lambda, deformation_gradient);

  try {
    // Check if matrices dimensions are correct
    if ((grad_shapefn.rows() != nodal_coordinates.rows()) ||
        (xi.size() != nodal_coordinates.cols()))
      throw std::runtime_error(
          "Jacobian calculation: Incorrect dimension of xi and "
          "nodal_coordinates");
  } catch (std::exception& exception) {
    console_->error("{} #{}: {}\n", __FILE__, __LINE__, exception.what());
    return Eigen::Matrix<double, Tdim, Tdim>::Zero();
  }

  // Jacobian dx_i/dxi_j
  return (grad_shapefn.transpose() * nodal_coordinates);
}

//! Compute Jacobian local with particle size and deformation gradient
template <unsigned Tdim>
inline Eigen::Matrix<double, Tdim, Tdim>
    mpm::TriangleLMEElement<Tdim>::jacobian_local(
        const VectorDim& xi, const Eigen::MatrixXd& nodal_coordinates,
        VectorDim& lambda, const MatrixDim& deformation_gradient) const {
  // Jacobian dx_i/dxi_j
  return mpm::TriangleElement<2, 3>::jacobian(xi, nodal_coordinates, lambda,
                                              deformation_gradient);
}

//! Compute natural coordinates of a point (analytical)
template <unsigned Tdim>
inline Eigen::Matrix<double, Tdim, 1>
    mpm::TriangleLMEElement<Tdim>::natural_coordinates_analytical(
        const VectorDim& point,
        const Eigen::MatrixXd& nodal_coordinates) const {
  // Local point coordinates
  Eigen::Matrix<double, 2, 1> xi;
  xi.fill(std::numeric_limits<double>::max());
  throw std::runtime_error(
      "Analytical solution for TriLME<Tdim> has "
      "not been "
      "implemented");
  return xi;
}
