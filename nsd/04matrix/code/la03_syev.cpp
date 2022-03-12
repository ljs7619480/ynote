#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>

#ifdef HASMKL
#include <mkl_lapack.h>
#include <mkl_lapacke.h>
#else // HASMKL
#ifdef __MACH__
#include <clapack.h>
#include <Accelerate/Accelerate.h>
#endif // __MACH__
#endif // HASMKL

class Matrix {

public:

    Matrix(size_t nrow, size_t ncol, bool column_major)
      : m_nrow(nrow), m_ncol(ncol), m_column_major(column_major)
    {
        reset_buffer(nrow, ncol);
    }

    Matrix
    (
        size_t nrow, size_t ncol, bool column_major
      , std::vector<double> const & vec
    )
      : m_nrow(nrow), m_ncol(ncol), m_column_major(column_major)
    {
        reset_buffer(nrow, ncol);
        (*this) = vec;
    }

    Matrix & operator=(std::vector<double> const & vec)
    {
        if (size() != vec.size())
        {
            throw std::out_of_range("number of elements mismatch");
        }

        size_t k = 0;
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = vec[k];
                ++k;
            }
        }

        return *this;
    }

    Matrix(Matrix const & other)
      : m_nrow(other.m_nrow), m_ncol(other.m_ncol)
      , m_column_major(other.m_column_major)
    {
        reset_buffer(other.m_nrow, other.m_ncol);
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = other(i,j);
            }
        }
    }

    Matrix & operator=(Matrix const & other)
    {
        if (this == &other) { return *this; }
        if (m_nrow != other.m_nrow || m_ncol != other.m_ncol)
        {
            reset_buffer(other.m_nrow, other.m_ncol);
        }
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = other(i,j);
            }
        }
        return *this;
    }

    Matrix(Matrix && other)
      : m_nrow(other.m_nrow), m_ncol(other.m_ncol)
      , m_column_major(other.m_column_major)
    {
        reset_buffer(0, 0);
        std::swap(m_buffer, other.m_buffer);
    }

    Matrix & operator=(Matrix && other)
    {
        if (this == &other) { return *this; }
        reset_buffer(0, 0);
        std::swap(m_nrow, other.m_nrow);
        std::swap(m_ncol, other.m_ncol);
        std::swap(m_buffer, other.m_buffer);
        m_column_major = other.m_column_major;
        return *this;
    }

    ~Matrix()
    {
        reset_buffer(0, 0);
    }

    double   operator() (size_t row, size_t col) const
    {
        return m_buffer[index(row, col)];
    }
    double & operator() (size_t row, size_t col)
    {
        return m_buffer[index(row, col)];
    }

    size_t nrow() const { return m_nrow; }
    size_t ncol() const { return m_ncol; }

    size_t size() const { return m_nrow * m_ncol; }
    double buffer(size_t i) const { return m_buffer[i]; }
    std::vector<double> buffer_vector() const
    {
        return std::vector<double>(m_buffer, m_buffer+size());
    }

    double * data() const { return m_buffer; }

private:

    size_t index(size_t row, size_t col) const
    {
        if (m_column_major) { return row          + col * m_nrow; }
        else                { return row * m_ncol + col         ; }
    }

    void reset_buffer(size_t nrow, size_t ncol)
    {
        if (m_buffer) { delete[] m_buffer; }
        const size_t nelement = nrow * ncol;
        if (nelement) { m_buffer = new double[nelement]; }
        else          { m_buffer = nullptr; }
        m_nrow = nrow;
        m_ncol = ncol;
    }

    size_t m_nrow = 0;
    size_t m_ncol = 0;
    bool m_column_major = false;
    double * m_buffer = nullptr;

};

std::ostream & operator << (std::ostream & ostr, Matrix const & mat)
{
    for (size_t i=0; i<mat.nrow(); ++i)
    {
        ostr << std::endl << " ";
        for (size_t j=0; j<mat.ncol(); ++j)
        {
            ostr << " " << std::setw(2) << mat(i, j);
        }
    }

    ostr << std::endl << " data: ";
    for (size_t i=0; i<mat.size(); ++i)
    {
        ostr << " " << std::setw(2) << mat.data()[i];
    }

    return ostr;
}

std::ostream & operator << (std::ostream & ostr, std::vector<double> const & vec)
{
    for (size_t i=0; i<vec.size(); ++i)
    {
        std::cout << " " << vec[i];
    }

    return ostr;
}

/*
 * See references:
 * * https://software.intel.com/en-us/mkl-developer-reference-c-syev
 * * https://software.intel.com/sites/products/documentation/doclib/mkl_sa/11/mkl_lapack_examples/lapacke_dsyev_row.c.htm
 */
int main(int argc, char ** argv)
{
    const size_t n = 3;
    int status;

    std::cout << ">>> Solve Ax=lx (row major, A symmetric)" << std::endl;
    Matrix mat(n, n, /* column_major */ true);
    mat(0,0) = 3; mat(0,1) = 5; mat(0,2) = 2;
    mat(1,0) = 5; mat(1,1) = 1; mat(1,2) = 3;
    mat(2,0) = 2; mat(2,1) = 3; mat(2,2) = 2;
    std::vector<double> w(n);

    std::cout << "A:" << mat << std::endl;

#if !defined(HASMKL) || defined(NOMKL)
    {
        char jobz = 'V';
        char uplo = 'U';
        int nn = n;
        int matncol = mat.ncol();
        int lwork = 3*n;
        std::vector<double> work(lwork);

        dsyev_(
            &jobz
          , &uplo
          , &nn // int * n: number of linear equation
          , mat.data() // double *: a
          , &matncol // int *: lda
          , w.data() // double *: w
          , work.data() // double *: work array
          , &lwork // int *: lwork
          , &status
          // for column major matrix, ldb remains the leading dimension.
        );
    }
#else // HASMKL NOMKL
    status = LAPACKE_dsyev(
        LAPACK_COL_MAJOR // int matrix_layout
      , 'V' // char jobz;
            // 'V' to compute both eigenvalues and eigenvectors,
            // 'N' only eigenvalues
      , 'U' // char uplo;
            // 'U' use the upper triangular of input a,
            // 'L' use the lower
      , n // lapack_int n
      , mat.data() // double * a
      , mat.nrow() // lapack_int lda
      , w.data() // double * w
    );
#endif // HASMKL NOMKL

    std::cout << "dsyev status: " << status << std::endl;
    std::cout << "eigenvalues: " << w << std::endl;
    std::cout << "eigenvectors:" << mat << std::endl;

    return 0;
}

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
