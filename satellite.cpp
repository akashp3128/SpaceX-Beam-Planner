#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace std;

// Define constants
const int NUM_COLORS = 4;
const double MIN_ANGLE_DIFF_SAME_SATELLITE = 10.0 * M_PI / 180.0;
const double MIN_ANGLE_DIFF_OTHER_SATELLITE = 20.0 * M_PI / 180.0;
const double MAX_ANGLE_FROM_VERTICAL = 90.0 * M_PI / 180.0;

struct Vector3 {
    double x, y, z;
    Vector3(double x_ = 0.0, double y_ = 0.0, double z_ = 0.0) : x(x_), y(y_), z(z_) {}
};

struct User {
    Vector3 position;
};

struct Satellite {
    Vector3 position;
    bool isStarlink;
    Satellite() : position(Vector3()), isStarlink(false) {} // Default constructor
    Satellite(const Vector3& pos, bool isStarlink_) : position(pos), isStarlink(isStarlink_) {}
};

double angle_between(const Vector3& v1, const Vector3& v2) {
    double dot_product = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    double magnitude_product = sqrt(v1.x * v1.x + v1.y * v1.y + v1.z * v1.z) *
                                sqrt(v2.x * v2.x + v2.y * v2.y + v2.z * v2.z);
    return acos(dot_product / magnitude_product);
}

bool can_assign_beam(const User& user, const Satellite& satellite, const vector<Satellite>& starlink_satellites, const vector<Satellite>& other_satellites, int color, const vector<pair<int, int>>& assigned_beams) {
    Vector3 user_to_satellite(satellite.position.x - user.position.x, satellite.position.y - user.position.y, satellite.position.z - user.position.z);
    double user_to_satellite_magnitude = sqrt(user_to_satellite.x * user_to_satellite.x + user_to_satellite.y * user_to_satellite.y + user_to_satellite.z * user_to_satellite.z);
    user_to_satellite.x /= user_to_satellite_magnitude;
    user_to_satellite.y /= user_to_satellite_magnitude;
    user_to_satellite.z /= user_to_satellite_magnitude;

    double angle_from_vertical = angle_between(user_to_satellite, Vector3(0.0, 0.0, 1.0));
    if (angle_from_vertical > MAX_ANGLE_FROM_VERTICAL) {
        return false;
    }

    for (const auto& beam : assigned_beams) {
        int other_satellite_idx = beam.first;
        int other_beam_color = beam.second;
        if (other_beam_color == color && other_satellite_idx != -1) {
            const Satellite& other_satellite = starlink_satellites[other_satellite_idx];
            Vector3 user_to_other_satellite(other_satellite.position.x - user.position.x, other_satellite.position.y - user.position.y, other_satellite.position.z - user.position.z);
            double user_to_other_satellite_magnitude = sqrt(user_to_other_satellite.x * user_to_other_satellite.x + user_to_other_satellite.y * user_to_other_satellite.y + user_to_other_satellite.z * user_to_other_satellite.z);
            user_to_other_satellite.x /= user_to_other_satellite_magnitude;
            user_to_other_satellite.y /= user_to_other_satellite_magnitude;
            user_to_other_satellite.z /= user_to_other_satellite_magnitude;
            double angle_diff = angle_between(user_to_satellite, user_to_other_satellite);
            if (angle_diff < MIN_ANGLE_DIFF_SAME_SATELLITE) {
                return false;
            }
        } else if (other_satellite_idx != -1 && !satellite.isStarlink) {
            const Satellite& other_satellite = other_satellites[beam.first];
            Vector3 user_to_other_satellite(other_satellite.position.x - user.position.x, other_satellite.position.y - user.position.y, other_satellite.position.z - user.position.z);
            double user_to_other_satellite_magnitude = sqrt(user_to_other_satellite.x * user_to_other_satellite.x + user_to_other_satellite.y * user_to_other_satellite.y + user_to_other_satellite.z * user_to_other_satellite.z);
            user_to_other_satellite.x /= user_to_other_satellite_magnitude;
            user_to_other_satellite.y /= user_to_other_satellite_magnitude;
            user_to_other_satellite.z /= user_to_other_satellite_magnitude;
            double angle_between_beams = angle_between(user_to_satellite, user_to_other_satellite);
            if (angle_between_beams < MIN_ANGLE_DIFF_OTHER_SATELLITE) {
                return false;
            }
        }
    }

    return true;
}

void readInputData(const string& filename, vector<User>& users, vector<Satellite>& starlink_satellites, vector<Satellite>& other_satellites) {
    ifstream input_file(filename);
    if (!input_file) {
        cerr << "Error: Unable to open input file " << filename << endl;
        return;
    }

    string line;
    while (getline(input_file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        char type[20];
        int id;
        double x, y, z;

        if (sscanf(line.c_str(), "%s %d %lf %lf %lf", type, &id, &x, &y, &z) != 5) {
            cerr << "Error: Invalid data format: " << line << endl;
            continue;
        }

        if (strcmp(type, "user") == 0) {
            users.emplace_back(User{Vector3(x, y, z)});
        } else if (strcmp(type, "sat") == 0) {
            starlink_satellites.emplace_back(Satellite(Vector3(x, y, z), true));
        } else if (strcmp(type, "interferer") == 0) {
            other_satellites.emplace_back(Satellite(Vector3(x, y, z), false));
        } else {
            cerr << "Error: Unknown type: " << type << endl;
        }
    }

    input_file.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input_file> [directory]" << endl;
        return 1;
    }

    string input_file = argv[1];
    string directory_path;
    if (argc > 2) {
        directory_path = argv[2];
    }

    if (!directory_path.empty()) {
        // Process all files in the directory
        for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                vector<User> users;
                vector<Satellite> starlink_satellites;
                vector<Satellite> other_satellites;

                readInputData(entry.path().string(), users, starlink_satellites, other_satellites);

                vector<pair<int, int>> assigned_beams; // (satellite_idx, color)
                int num_covered_users = 0;

                for (int user_idx = 0; user_idx < users.size(); ++user_idx) {
                    const User& user = users[user_idx];
                    bool covered = false;

                    for (int sat_idx = 0; sat_idx < starlink_satellites.size(); ++sat_idx) {
                        const Satellite& satellite = starlink_satellites[sat_idx];
                        int available_color = -1;

                        for (int color = 0; color < NUM_COLORS; ++color) {
                            if (can_assign_beam(user, satellite, starlink_satellites, other_satellites, color, assigned_beams)) {
                                available_color = color;
                                break;
                            }
                        }

                        if (available_color != -1) {
                            assigned_beams.emplace_back(sat_idx, available_color);
                            covered = true;
                            num_covered_users++;
                            break;
                        }
                    }
                }

                cout << "Number of covered users for " << entry.path().string() << ": " << num_covered_users << endl;
            }
        }
    } else {
        // Process the single input file
        vector<User> users;
        vector<Satellite> starlink_satellites;
        vector<Satellite> other_satellites;

        readInputData(input_file, users, starlink_satellites, other_satellites);

        vector<pair<int, int>> assigned_beams; // (satellite_idx, color)
        int num_covered_users = 0;

        for (int user_idx = 0; user_idx < users.size(); ++user_idx) {
            const User& user = users[user_idx];
            bool covered = false;

            for (int sat_idx = 0; sat_idx < starlink_satellites.size(); ++sat_idx) {
                const Satellite& satellite = starlink_satellites[sat_idx];
                int available_color = -1;

                for (int color = 0; color < NUM_COLORS; ++color) {
                    if (can_assign_beam(user, satellite, starlink_satellites, other_satellites, color, assigned_beams)) {
                        available_color = color;
                        break;
                    }
                }

                if (available_color != -1) {
                    assigned_beams.emplace_back(sat_idx, available_color);
                    covered = true;
                    num_covered_users++;
                    break;
                }
            }
        }

        cout << "Number of covered users for " << input_file << ": " << num_covered_users << endl;
    }

    return 0;
}