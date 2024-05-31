#pragma once

#ifdef EXECUTION_REGRESSION

#include "http_session.h"
#include <stdint.h>

static inline uint64_t
get_regression_prediction(struct http_session *session)
{
	/* Default Pre-processing - Extract payload size */
	const int payload_size = session->http_request.body_length;

	const double regression_params[2] = {payload_size, session->paregression_paramram2};

	/* Perform Linear Regression using the factors provided by the regressor performed AoT on Matlab using training
	 * tenant-given dataset */
	const struct regression_model model      = session->route->regr_model;
	const uint64_t                prediction = (regression_params[0] / model.scale * model.beta1
                                     + regression_params[1] / model.scale * model.beta2)
	                            + model.bias;

	return prediction;
}

#endif