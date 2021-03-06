/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1.
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method
   *   (and others in this file).
   */
  num_particles = 100;  // TODO: Set the number of particles

  // Instantiate random distributions
  std::normal_distribution<double> dist_x(x, std[0]);
  std::normal_distribution<double> dist_y(y, std[1]);
  std::normal_distribution<double> dist_theta(theta, std[2]);

  // Instantiate random generator engine
  std::default_random_engine gen;

  for (int i = 0; i < num_particles; ++i) {

      // generate a particle
      Particle particle;
      particle.id = i;
      particle.x = dist_x(gen);
      particle.y = dist_y(gen);
      particle.theta = dist_theta(gen);
      particle.weight = 1.0f;

      // add generated particle to particles
      particles.push_back(particle);

      // add particle weight to weights
      // probably not needed
      //weights.push_back(particle.weight)
  }

  is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[],
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */

   // random generator engine
   std::default_random_engine gen;

   // Instantiate random distributions
   std::normal_distribution<double> noise_x(0, std_pos[0]);
   std::normal_distribution<double> noise_y(0, std_pos[1]);
   std::normal_distribution<double> noise_theta(0, std_pos[2]);

   for (int i = 0; i < num_particles; i++) {
       Particle p = particles[i];

       if (fabs(yaw_rate) < 0.0001) {

          p.x = p.x + velocity * delta_t * cos(p.theta);
          p.y = p.y + velocity * delta_t * sin(p.theta);
        }
        else
        {
          p.x = p.x + velocity / yaw_rate * (sin(p.theta + yaw_rate * delta_t) - sin(p.theta));
          p.y = p.y + velocity / yaw_rate * (cos(p.theta) - cos(p.theta + yaw_rate * delta_t));
        }
        p.theta = p.theta + delta_t * yaw_rate;

        //add noise
        p.x += noise_x(gen);
        p.y += noise_y(gen);
        p.theta += noise_theta(gen);

        particles[i] = p;
   }

}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted,
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each
   *   observed measurement and assign the observed measurement to this
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will
   *   probably find it useful to implement this method and use it as a helper
   *   during the updateWeights phase.
   */

   // If no predicted landmark is present, no association is made
   if (predicted.size() == 0) {
     return;
   }

   for (int i = 0; i < observations.size(); ++i)
   {
       double min_dist = std::numeric_limits<double>::max();
       int id = -1;

       for (int j = 0; j < predicted.size(); ++j)
       {
           double len = dist(predicted[j].x, predicted[j].y, observations[i].x, observations[i].y);

           if (len < min_dist) {
               min_dist = len;
               id = predicted[j].id;
           }
       }
       observations[i].id = id;
   }

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
                                   const vector<LandmarkObs> &observations,
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian
   *   distribution. You can read more about this distribution here:
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system.
   *   Your particles are located according to the MAP'S coordinate system.
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
   double sig_x = std_landmark[0];
   double sig_y = std_landmark[1];

   for (int i = 0; i < particles.size(); ++i) {
       Particle &p = particles[i];

       // transform observations from particle coordinates to map coordinates
       std::vector<LandmarkObs> map_obs;
       LandmarkObs obs;

       for (int j = 0; j < observations.size(); ++j) {
           obs = observations[j];
           double x_m = p.x + cos(p.theta) * obs.x - sin(p.theta) * obs.y;
           double y_m = p.y + sin(p.theta) * obs.x + cos(p.theta) * obs.y;
           map_obs.push_back(LandmarkObs{obs.id, x_m, y_m});
       }

       // predict measurements of all landmarks whithin the sensor_range of each particle
       std::vector<LandmarkObs> in_range_landmarks;

       int num_landmarks = map_landmarks.landmark_list.size(); // total map landmarks

       for (int l = 0; l < num_landmarks; ++l) {
           Map::single_landmark_s lm = map_landmarks.landmark_list[l];
           double lm_dist = dist(p.x, p.y, lm.x_f, lm.y_f);
           if (lm_dist < sensor_range) {
               in_range_landmarks.push_back(LandmarkObs{lm.id_i, lm.x_f, lm.y_f});
           }
       }

       // dataAssociation
       dataAssociation(in_range_landmarks, map_obs);

       // probability for each observation
       double total_prob = 1.0;

       for (int i = 0; i < map_obs.size(); ++i) {

           LandmarkObs obs = map_obs[i];
           LandmarkObs pred;

           for (int j = 0; j < in_range_landmarks.size(); ++j){
               if (in_range_landmarks[j].id == obs.id) {
                   pred = in_range_landmarks[j];
                   break;
               }
           }
           // Multivariate normal distribution
           double pxy = (0.5 / (M_PI * sig_x * sig_y)) *
                        exp(-(pow(pred.x - obs.x, 2) / (2 * pow(sig_x, 2))
                            + pow(pred.y - obs.y, 2) / (2 * pow(sig_y, 2))));
           total_prob *= pxy;
       }

       // set weight to the product of all probablities
       p.weight = total_prob;
   }

}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional
   *   to their weight.
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
   std::default_random_engine gen;

   for (int i = 0; i < num_particles; i++) {
       weights.push_back(particles[i].weight);
   }

   // use std::discrete_distribution on weights
   std::discrete_distribution<> discrete_dist(weights.begin(), weights.end());

   std::vector<Particle> new_particles;

    for (int i = 0; i < num_particles; i++) {
        int idx = discrete_dist(gen);
        new_particles.push_back(particles[idx]);
   }
   weights.clear();
   particles = new_particles;

}

void ParticleFilter::SetAssociations(Particle& particle,
                                     const vector<int>& associations,
                                     const vector<double>& sense_x,
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association,
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}
