#include <SDL.h>
#include <stdio.h>
#include <cmath>
#include <vector>
#include <fstream>
#include <string>

#define screenScale	1			// screen scalar
#define res			6			// resolution scalar
#define SW			160*res		// screen width
#define SH			120*res		// screen height
#define SW2			SW/2		// screen half width
#define SH2			SH/2		// screen half height
#define FOV			90			// field of view
#define disToScr	SW2/(tan(FOV*M_PI/360.0))	// distance of screen from camera
#define MAXBOUNCES	4			// bro i dont have a NVIDIA 69420 graphics card
#define RENDERS		100			// gotta get that smooth shading

const double INF = 1e9;
const double EPS = 1e-4;

int sign(double x) {
	if (x < 0.0) return -1;
	if (x > 0.0) return 1;
	return 0;
}

SDL_Window* gWindow = NULL;

SDL_Renderer* gRenderer = NULL;

unsigned int rngState = 69420;

struct vec3 {
	double x, y, z;

	vec3() { x = 0.0; y = 0.0; z = 0.0; }
	vec3(double _x) { x = _x; y = _x; z = _x; }
	vec3(double _x, double _y, double _z) { x = _x; y = _y; z = _z; }

	bool operator==(const vec3& other) const { return (x == other.x) && (y == other.y) && (z == other.z); }
	vec3 operator+(const vec3& other) const { return vec3(x + other.x, y + other.y, z + other.z); }
	vec3 operator+(const double other) const { return vec3(x + other, y + other, z + other); }
	vec3 operator-(const vec3& other) const { return vec3(x - other.x, y - other.y, z - other.z); }
	vec3 operator-(const double other) const { return vec3(x - other, y - other, z - other); }
	vec3 operator*(const vec3& other) const { return vec3(x * other.x, y * other.y, z * other.z); }
	vec3 operator*(const double other) const { return vec3(x * other, y * other, z * other); }
	vec3 operator/(const vec3& other) const { return vec3(x / other.x, y / other.y, z / other.z); }
	vec3 operator/(const double other) const { return vec3(x / other, y / other, z / other); }

	double dot(const vec3& other) const { return x * other.x + y * other.y + z * other.z; }
	double length() { return sqrt((*this).dot(*this)); }
	vec3& norm() { return *this = *this / (*this).length(); }
};

struct Ray {
	vec3 origin, dir;
	Ray(vec3 _origin, vec3 _dir) {
		origin = _origin;
		dir = _dir.norm();
	}
};

class Obj {
public:
	vec3 color = vec3();
	double emmision = 0.0;
	int BRDFtype = 0;
	void setMaterial(vec3 _color, double _emmision, int type) {
		color = _color; emmision = _emmision; BRDFtype = type;
	}
	virtual double intersect(const Ray&) const = 0;
	virtual vec3 normal(const vec3&) const = 0;
};

class Sphere : public Obj {
public:
	vec3 pos;
	double radius;
	Sphere(vec3 _pos, double _radius) {
		pos = _pos; radius = _radius;
	}

	double intersect(const Ray& ray) const {
		vec3 offsetRayOrigin = ray.origin - pos;
		double b = 2.0 * offsetRayOrigin.dot(ray.dir);
		double c = offsetRayOrigin.dot(offsetRayOrigin) - (radius * radius);

		double discriminant = b * b - 4 * c;
		if (discriminant < 0) return 0;
		else discriminant = sqrt(discriminant);
		double sol1 = -b + discriminant;
		double sol2 = -b - discriminant;
		return (sol2 > EPS) ? sol2 / 2 : ((sol1 > EPS) ? sol1 / 2 : 0);
	}

	vec3 normal(const vec3& point) const {
		return (point - pos).norm();
	}
};

class Plane : public Obj {
public:
	vec3 pos;
	vec3 n;
	Plane(vec3 _pos, vec3 _n) {
		pos = _pos, n = _n;
	}

	double intersect(const Ray& ray) const {
		vec3 offsetRayOrigin = ray.origin - pos;
		double ABN = offsetRayOrigin.dot(n);
		double sol = - 1.0 * ABN / n.dot(ray.dir);
		return (sol > EPS) ? sol : 0;
	}

	vec3 normal(const vec3& point) const {
		return n;
	}
};

struct Intersection {
	double t;
	Obj* object;
	Intersection() { t = 0; object = nullptr; }
	Intersection(double _t, Obj* _object) { t = _t; object = _object; }
	operator bool() { return object != nullptr; }
};

class Scene {
	std::vector<Obj*> objects;
public:
	void add(Obj* object, vec3 color, double emmision, int BRDFtype) {
		object->setMaterial(color, emmision, BRDFtype);
		objects.push_back(object);
	}
	Intersection intersect(const Ray& ray) const {
		Intersection closestIntersection;
		closestIntersection.t = INF;
		for (auto iter = objects.begin(); iter != objects.end(); ++iter) {
			double t = (*iter)->intersect(ray);
			if (t > EPS && t < closestIntersection.t) {
				closestIntersection.t = t;
				closestIntersection.object = *iter;
			}
		}
		return closestIntersection;
	}
}; Scene scene;

//returns random double between 0 and 1
double RandomValue(unsigned int& state) {
	state = state * 747796405 + 2891336453;
	unsigned int result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
	result = (result >> 22) ^ result;
	return result / 4294967295.0;
}

double RandomValueNormalDistribution(unsigned int& state) {
	double theta = 2.0 * M_PI * RandomValue(state);
	double rho = sqrt(-2.0 * log(RandomValue(state)));
	return rho * cos(theta);
}

vec3 RandomDirection(unsigned int& state) {
	double x = RandomValueNormalDistribution(state);
	double y = RandomValueNormalDistribution(state);
	double z = RandomValueNormalDistribution(state);
	return vec3(x, y, z).norm();
}

vec3 RandomHemisphereDirection(vec3 normal, unsigned int& state) {
	vec3 dir = RandomDirection(state);
	return dir * sign(normal.dot(dir));
}

void Trace(Ray& ray, vec3& color, vec3& incominglight, int depth) {
	if (depth >= MAXBOUNCES) return;

	Intersection intersection = scene.intersect(ray);
	if (!intersection) return;

	if (intersection.object->BRDFtype == 0) { // No shading
		color.x = intersection.object->color.x;
		color.y = intersection.object->color.y;
		color.z = intersection.object->color.z;
	}
	else if (intersection.object->BRDFtype == 1) {	// Phong shading
		vec3 ambientColor = intersection.object->color;
		vec3 diffuseColor = intersection.object->color;
		vec3 specularColor = vec3(1.0);
		double ambient = 0.2;
		double diffuse = 0.6;
		double specular = 0.0;
		double shinyness = 1.0;

		vec3 p = ray.origin + (ray.dir * intersection.t);
		vec3 L = (vec3(2.0, 2.0, -2.0)-p).norm();
		vec3 N = intersection.object->normal(p);
		vec3 V = (vec3(0.0) - p).norm();
		vec3 R = vec3(0.0) - L + N * 2.0 * L.dot(N);

		color = color * ((ambientColor * ambient) + (diffuseColor * diffuse * L.dot(N)) + (specularColor * specular * pow(V.dot(R), shinyness)));
		incominglight = incominglight + color;
	}
	else if (intersection.object->BRDFtype == 2) { // Reflection
		vec3 p = ray.origin + (ray.dir * intersection.t);
		vec3 N = intersection.object->normal(p);
		ray.dir = ray.dir - N * (ray.dir.dot(N) * 2.0);
		ray.origin = p;
		color = color * intersection.object->color;
		Trace(ray, color, incominglight, depth + 1);
	}
	else if (intersection.object->BRDFtype == 3) { // Diffuse
		color = color * intersection.object->color;
		incominglight = incominglight + color * intersection.object->emmision;
		vec3 p = ray.origin + (ray.dir * intersection.t);
		vec3 N = intersection.object->normal(p);
		ray.dir = (N + RandomDirection(rngState)).norm();
		ray.origin = p;
		Trace(ray, color, incominglight, depth + 1);
	}
	return;
}

void drawPix(int x, int y, vec3 color) {
	int r, g, b;
	r = (int)(255 * SDL_clamp(color.x, 0.0, 1.0));
	g = (int)(255 * SDL_clamp(color.y, 0.0, 1.0));
	b = (int)(255 * SDL_clamp(color.z, 0.0, 1.0));
	SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
	SDL_RenderDrawPoint(gRenderer, x, y);
}

void close() {
	SDL_DestroyRenderer(gRenderer);
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;
	gRenderer = NULL;

	//Quit SDL subsystems
	SDL_Quit();
}

int framesRendered = 0;
vec3 pix[SW][SH];
void render() {

	scene.add(new Sphere(vec3(-2.5, -2.5, 8.0), 1.5), vec3(1.0), 0.0, 2);
	scene.add(new Sphere(vec3(0.0, 6.0, 5.5), 2.5), vec3(1.0), 10.0, 3);
	scene.add(new Sphere(vec3(2.0, 1.5, 6.0), 1), vec3(0.4, 0.8, 0.4), 0.0, 3);

	scene.add(new Plane(vec3(0.0, -4.0, 0.0), vec3(0.0, 1.0, 0.0)), vec3(0.9, 0.9, 0.9), 0.0, 3);
	scene.add(new Plane(vec3(0.0, 4.0, 0.0), vec3(0.0, -1.0, 0.0)), vec3(0.9, 0.9, 0.9), 0.0, 3);
	scene.add(new Plane(vec3(4.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0)), vec3(0.4, 0.4, 0.8), 0.0, 2);
	scene.add(new Plane(vec3(-4.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0)), vec3(0.8, 0.4, 0.4), 0.0, 2);
	scene.add(new Plane(vec3(0.0, 0.0, 9.5), vec3(0.0, 0.0, -1.0)), vec3(0.9, 0.9, 0.9), 0.0, 3);
	scene.add(new Plane(vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, 1.0)), vec3(0.0), 0.0, 3);

	int i, j;
	for (i = 0; i < SW; ++i) {
		for (j = 0; j < SH; ++j) {
			Ray ray = Ray(vec3(0.0), vec3(i - SW2, SH2 - j, disToScr));
			vec3 color = vec3(1.0);
			vec3 incominglight = 0.0;
			Trace(ray, color, incominglight, 0);
			pix[i][j] = (pix[i][j] * framesRendered + incominglight) / (framesRendered + 1);
			//printf("%f\n", pix[i][j].x);
		}
		//printf("rendered column %i\n", i);
	}
	framesRendered++;
	printf("rendered %i frames\n", framesRendered);
}

void saveasppm(int frame) {
	std::string filename = "Daniel_Li_updated_raytracer_"+ std::to_string(frame) + "frames_" + std::to_string(frame*SW*SH) + "photons" + ".ppm";
	std::ofstream ofs(filename, std::ios_base::out | std::ios_base::binary);
	ofs << "P6\n" << SW << ' ' << SH << "\n255\n";
	for (auto j = 0u; j < SH; ++j) {
		for (auto i = 0u; i < SW; ++i) {
			int r, g, b;
			vec3 color = pix[i][j];
			r = (int)(255 * SDL_clamp(color.x, 0.0, 1.0));
			g = (int)(255 * SDL_clamp(color.y, 0.0, 1.0));
			b = (int)(255 * SDL_clamp(color.z, 0.0, 1.0));
			ofs << static_cast<char>(r)
				<< static_cast<char>(g)
				<< static_cast<char>(b);
		}
	}
}

int main(int argc, char* args[]) {
	for (int i = 0; i < RENDERS; i++) {
		render();
		if (i == 0 || i == 9 || i == 49 || i == 99) {
			saveasppm(i+1);
		}
	}

	//The surface contained by the window
	SDL_Surface* screenSurface = NULL;

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
	}
	else {

		//Create window
		gWindow = SDL_CreateWindow("Path Tracer | FallingSky65", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenScale * SW, screenScale * SH, SDL_WINDOW_SHOWN);
		if (gWindow == NULL) {
			printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		}
		else {
			//Create renderer for window
			gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
			SDL_RenderSetScale(gRenderer, screenScale, screenScale);

			if (gRenderer == NULL) {
				printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
			}
			else {

				/*
				//Get window surface
				screenSurface = SDL_GetWindowSurface( gWindow );

				//Fill the surface white
				SDL_FillRect( screenSurface, NULL, SDL_MapRGB( screenSurface->format, 0xFF, 0xFF, 0xFF ) );

				//Update the surface
				SDL_UpdateWindowSurface( gWindow );
				*/

				SDL_SetRenderDrawColor(gRenderer, 255, 0, 0, 255);
				SDL_RenderClear(gRenderer);
				int i, j;
				for (i = 0; i < SW; ++i) {
					for (j = 0; j < SH; ++j) {
						int r, g, b;
						vec3 color = pix[i][j];
						r = (int)(255 * SDL_clamp(color.x, 0.0, 1.0));
						g = (int)(255 * SDL_clamp(color.y, 0.0, 1.0));
						b = (int)(255 * SDL_clamp(color.z, 0.0, 1.0));
						SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
						SDL_RenderDrawPoint(gRenderer, i, j);
					}
				}
				SDL_RenderPresent(gRenderer);

				printf("Simulated %i lightrays/photons\n", SW * SH * RENDERS);

				//Hack to get window to stay up
				SDL_Event e;
				bool quit = false;
				while (!quit) {
					while (SDL_PollEvent(&e)) {
						if (e.type == SDL_QUIT) quit = true;
					}
				}
			}
		}
	}
	close();

	return 0;
}