#include <Rcpp.h>
using namespace Rcpp;

// Main code for text label placement -----------------------------------------

typedef struct {
  double x, y;
} Point;

Point operator -(const Point& a, const Point& b) {
  Point p = {a.x - b.x, a.y - b.y};
  return p;
}

Point operator +(const Point& a, const Point& b) {
  Point p = {a.x + b.x, a.y + b.y};
  return p;
}

Point operator /(const Point& a, const double& b) {
  Point p = {a.x / b, a.y / b};
  return p;
}

Point operator *(const double& b, const Point& a) {
  Point p = {a.x * b, a.y * b};
  return p;
}

Point operator *(const Point& a, const double& b) {
  Point p = {a.x * b, a.y * b};
  return p;
}

typedef struct {
  double x1, y1, x2, y2;
} Box;

Box operator +(const Box& b, const Point& p) {
  Box c = {b.x1 + p.x, b.y1 + p.y, b.x2 + p.x, b.y2 + p.y};
  return c;
}

bool overlaps(Box a, Box b) {
  return
  b.x1 <= a.x2 &&
    b.y1 <= a.y2 &&
    b.x2 >= a.x1 &&
    b.y2 >= a.y1;
}

// [[Rcpp::export]]
DataFrame wordcloud_boxes(
    NumericMatrix data_points,
    NumericMatrix boxes,
    IntegerVector boxes_text,
    IntegerMatrix text_boxes,
    NumericVector xlim, NumericVector ylim,
    double eccentricity = 0.65,
    double rstep = 0.1, double tstep = 0.05,
    bool rm_outside = false) {

  if (NumericVector::is_na(rstep)) {
    rstep = 0.1;
  }
  if (NumericVector::is_na(tstep)) {
    tstep = 0.05;
  }

  int n_texts = text_boxes.nrow();
  int n_boxes = boxes.nrow();

  std::vector<bool> text_inside(n_texts);

  int iter = 0;
  bool i_overlaps = true;


  Point xbounds, ybounds;
  xbounds.x = xlim[0];
  xbounds.y = xlim[1];
  ybounds.x = ylim[0];
  ybounds.y = ylim[1];

  Box inside;
  inside.x1 = xlim[0];
  inside.y1 = ylim[0];
  inside.x2 = xlim[1];
  inside.y2 = ylim[1];


  std::vector<Point> current_centroids(n_texts);
  for (int i = 0; i < n_texts; i++) {
    current_centroids[i].x = data_points(i, 0);
    current_centroids[i].y = data_points(i, 1);
  }

  std::vector<Box> TextBoxes(n_boxes);

  Point d;
  double r;
  double rscale = ((xlim[1]-xlim[0])*(xlim[1]-xlim[0])+
                       (ylim[1]-ylim[0])*(ylim[1]-ylim[0])/(eccentricity * eccentricity));
  double theta;

  for (int i = 0; i < n_texts; i++) {
    Rcpp::checkUserInterrupt();
    i_overlaps = true;
    iter       = 0;
    r          = 0;
    theta      = R::runif(0, 2 * M_PI);
    d.x        = 0;
    d.y        = 0;
    Point PosOri = current_centroids[i];
    Point CurPos;
    Point corr;
    text_inside[i] = false;

    // Try to position the current text box
    while (i_overlaps && r < rscale) {
      iter += 1;
      i_overlaps = false;

      CurPos = PosOri + d;
      bool one_inside = false;
      for (int ii = text_boxes(i,0); ii < text_boxes(i,1); ii++) {
        TextBoxes[ii].x1 = CurPos.x + boxes(ii, 0);
        TextBoxes[ii].x2 = CurPos.x + boxes(ii, 2);
        TextBoxes[ii].y1 = CurPos.y + boxes(ii, 1);
        TextBoxes[ii].y2 = CurPos.y + boxes(ii, 3);
        one_inside = one_inside || overlaps(TextBoxes[ii], inside);
      }

      if (one_inside) {
        corr.x = 0;
        corr.y = 0;
        for (int ii = text_boxes(i,0); ii < text_boxes(i,1); ii++){
          if (TextBoxes[ii].x1 < xbounds.x) {
            corr.x = std::max(xbounds.x-TextBoxes[ii].x1,corr.x);
          }
          if (TextBoxes[ii].x2 > xbounds.y) {
            corr.x = std::min(xbounds.y-TextBoxes[ii].x2,corr.x);
          }
          if (TextBoxes[ii].y1 < ybounds.x) {
            corr.y = std::max(ybounds.x-TextBoxes[ii].y1,corr.y);
          }
          if (TextBoxes[ii].y2 > ybounds.y) {
            corr.y = std::min(ybounds.y-TextBoxes[ii].y2,corr.y);
          }
        }
        for (int ii = text_boxes(i,0); ii < text_boxes(i,1); ii++){
          TextBoxes[ii] = TextBoxes[ii] + corr;
        }
        CurPos = CurPos + corr;


        for (int ii = text_boxes(i,0); (!i_overlaps) && (ii < text_boxes(i,1)); ii++){
          for (int jj = 0 ; (!i_overlaps) && (jj < text_boxes(i,0)); jj++)
            if (overlaps(TextBoxes[ii], TextBoxes[jj])) {
              i_overlaps = true;
            }
        }
      } else {
        i_overlaps = true;
      }

      if (i_overlaps) {
        theta += tstep * (2 * M_PI);
        r     += rscale * rstep * tstep;
        d.x    = r * cos(theta);
        d.y    = r * sin(theta)*eccentricity;
      } else {
        current_centroids[i] = CurPos;
        text_inside[i] = true;
      }
    } // loop over already positioned boxes

  } // loop over texts

  NumericVector xs(n_texts);
  NumericVector ys(n_texts);

  int nb_bad = 0;
  for (int i = 0; i < n_texts; i++) {
    if (!text_inside[i]) { nb_bad++; }
    if (text_inside[i]||!rm_outside) {
      xs[i] = current_centroids[i].x;
      ys[i] = current_centroids[i].y;
    } else {
      xs[i] = NA_REAL;
      ys[i] = NA_REAL;
    }
  }

  if (nb_bad > 0) {
    if (nb_bad == 1) {
      if (rm_outside) {
        Rcpp::warning("One word could not fit on page. It has been removed.");
      } else {
        Rcpp::warning("One word could not fit on page. It has been placed at its original position.");
      }
    } else {
      if (rm_outside) {
        Rcpp::warning("Some words could not fit on page. They have been removed.");
      } else {
        Rcpp::warning("Some words could not fit on page. They have been placed at their original positions.");
      }
    }
  }


  return Rcpp::DataFrame::create(
    Rcpp::Named("x") = xs,
    Rcpp::Named("y") = ys
  );
}
