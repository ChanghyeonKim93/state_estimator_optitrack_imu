#include "error_state_kalman_filter.h"

// Initialize Static member variables
Mat33 ESKF::I33 = Mat33::Identity();
Mat44 ESKF::I44 = Mat44::Identity();
Mat33 ESKF::O33 = Mat33::Zero();
Mat44 ESKF::O44 = Mat44::Zero();
Mat34 ESKF::O34 = Mat34::Zero();
Mat43 ESKF::O43 = Mat43::Zero();    
Mat1515 ESKF::I1515 = Mat1515::Identity();

ESKF::ESKF()
: measurement_noise_(), process_noise_(), X_nom_(), dX_(), isInitialized_(false)
{
    // initialize error covariance matrix
    P_ = CovarianceMat::Identity()*0.005;

    // initialize Fi matrix
    Fi_ << O33, O33, O33, O33, 
           I33, O33, O33, O33,
           O33, I33, O33, O33,
           O33, O33, I33, O33,
           O33, O33, O33, I33;
           
    Vec3 ba_init(0.011, 0.007, 0.201);
    Vec3 bg_init(0.0043586,-0.0011758,-0.011671);

    X_nom_.setBiasAcc(ba_init);
    X_nom_.setBiasGyro(bg_init);

    std::cout << "Error State Kalman Filter - constructed\n";
};

ESKF::~ESKF(){
    std::cout << "Error State Kalman Filter - destructed\n";
};

void ESKF::predict(const Vec3& am, const Vec3& wm, double t_now){
    if(!isInitialized_) {
        std::cout << "ESKF - predict() : FILTER IS NOT INITIALIZED YET...\n";
        return;
    }

    // Do implementation
    std::cout << "Predict...\n";
    double dt = t_now - t_prev_;
    if(dt > 0.02) {
        std::cout << " MIGHT BE TIME MIGHT BE PASSED TOO LONG!... dt is set to 0.05 s.\n";
        dt = 0.02;
    }
    t_prev_ = t_now;

    // Do prediction    
    NominalState X_nom_predict;
    ErrorState   dX_predict;
    
    // 1. nominal state prediction
    predictNominal(X_nom_, am, wm, dt,
        X_nom_predict);

    // 2. error-state prediction
    predictError(X_nom_, dX_, am, wm, dt,
        dX_predict);

    // 3. Error Covariance propagation
    FMat F0;
    errorStateF(X_nom_, am, wm, 
        F0);
    
    FMat eF0dt;
    this->expm_FMat(F0,dt,5, 
        eF0dt);

    P_ = eF0dt * P_ * eF0dt.transpose() + Fi_*process_noise_.Q*Fi_.transpose();
    
    // replace the old with the new.
    X_nom_.replace(X_nom_predict);
    dX_.replace(dX_predict);
    
    X_nom_.show();

    std::cout <<"--------------------------" << std::endl;
};

void ESKF::updateOptitrack(const Vec3& p_observe, const Vec4& q_observe, double t_now){
   
    // Do implementation
    std::cout << "Update...\n";
    
    Vec3 p_obs;
    Vec4 q_obs;
    p_obs = fixed_param_.R_IB*p_observe;
    q_obs = geometry::q1_mult_q2(geometry::q1_mult_q2(fixed_param_.q_BI, q_observe), fixed_param_.q_IB);

    if(!isInitialized_){
        isInitialized_ = true;
        X_nom_.setPosition(p_obs);
        X_nom_.setQuaternion(q_obs);
        t_prev_ = t_now;
        t_init_ = t_now;

        std::cout << "FILTER INITIALIZED. time: " << t_now-t_init_;
        X_nom_.show();
        std::cout << std::endl;
        return;
    }

    Vec7 y;
    y << p_obs,q_obs;

    // calculate Linearized observation matrix
    HMat H;
    calcH(X_nom_, H);

    // Kalman gain
    KMat K;
    K = P_*H.transpose()*(H*P_*H.transpose() + measurement_noise_.R).inverse();
    
    Vec7 y_hat;
    y_hat << (X_nom_.p+dX_.dp),(geometry::q_right_mult(geometry::rotvec2q(dX_.dth))*X_nom_.q); 
    
    ErrorStateVec dX_update_vec;
    dX_update_vec = K*(y-y_hat);

    ErrorState dX_update;
    dX_update.replace(dX_update_vec);

    P_ = (I1515-K*H)*P_*(I1515-K*H).transpose() + K*measurement_noise_.R*K.transpose();

    // Injection of the observed error into the nominal
    X_nom_.injectErrorState(dX_update);

    // Reset dX
    dX_.replace(dX_update);

    // dg_ddX << I66, O63, O63,
    //           O36, I33-skewMat(0.5*dX_update.dth), O36,
    //           O66, O63, I66;
    // P_ = dg_ddX*P_*dg_ddX.transpose();
    X_nom_.show();
};

void ESKF::predictNominal(const NominalState& X_nom, const Vec3& am, const Vec3& wm, double dt, 
    NominalState& X_nom_update){
    Matrix3d R_B0Ik = geometry::q2r(X_nom.q);

    // Update nominal state
    Vec3 Adt = (R_B0Ik*(am-X_nom.ba)-fixed_param_.grav)*dt;
    X_nom_update.p  = X_nom.p + (X_nom.v + 0.5*Adt)*dt;
    X_nom_update.v  = X_nom.v + Adt;
    X_nom_update.q  = geometry::q_right_mult(geometry::rotvec2q((wm-X_nom.bg)*dt))*X_nom.q;
    X_nom_update.ba = X_nom.ba;
    X_nom_update.bg = X_nom.bg;
};

void ESKF::predictError(const NominalState& X_nom, const ErrorState& dX, const Vec3& am, const Vec3& wm, double dt,
    ErrorState& dX_update)
{
    FMat eF0dt;

    // Update nominal state
    // dX_update = eF0dt*dX;
};

void ESKF::errorStateF(const NominalState& X_nom, const Vec3& am, const Vec3& wm,
    FMat& res)
{
    Matrix3d R_B0Ik = geometry::q2r(X_nom.q);
    res.setZero();
    res << O33, I33, O33, O33, O33,
           O33, O33, -R_B0Ik*geometry::skewMat(am-X_nom.ba),-R_B0Ik, O33,
           O33, O33, -geometry::skewMat(wm-X_nom.bg), O33, -I33,
           O33, O33, O33, O33, O33, 
           O33, O33, O33, O33, O33;
};

void ESKF::expm_FMat(const FMat& F, const double& dt, int max_approx_order,
    expmFMat& expmF){

    expmF.setZero();

    double dt2 = dt*dt;
    double dt3 = dt2*dt;
    double dt4 = dt3*dt;
    double dt5 = dt4*dt;
    double mul2 = 0.5;
    double mul3 = 1.0/6.0;
    double mul4 = 1.0/24.0;
    double mul5 = 1.0/120.0;

    double dtm2 = mul2*dt2;
    double dtm3 = mul3*dt3;
    double dtm4 = mul4*dt4;
    double dtm5 = mul5*dt5;

    // fourth order approx?
    // expmF = I + F0*dt + 0.5*F0F0*dtdt  + 1/6*F0F0F0*dtdtdt
    //           + 1/24*F0F0F0F0*dtdtdtdt + 1/120*F0F0F0F0F0*dtdtdtdtdt;
    Matrix3d RBI       = -F.block<3,3>(3,9);

    Matrix3d skW     =  -F.block<3,3>(6,6);
    Matrix3d skW2    =  skW *skW;
    Matrix3d skW3    =  skW2*skW;
    Matrix3d skW4    =  skW3*skW;
    Matrix3d skW5    =  skW4*skW;

    Matrix3d RBI_skA       = -F.block<3,3>(3,6);
    Matrix3d RBI_skA_skW   = RBI_skA*skW;
    Matrix3d RBI_skA_skW2  = RBI_skA_skW*skW;
    Matrix3d RBI_skA_skW3  = RBI_skA_skW2*skW;
    Matrix3d RBI_skA_skW4  = RBI_skA_skW3*skW;


    Matrix3d I33 = Matrix3d::Identity();
    Matrix3d O33 = Matrix3d::Zero();

    if(max_approx_order < 2) max_approx_order = 2;
    if(max_approx_order > 5) max_approx_order = 5;

if(max_approx_order >= 0){
    // 0th order 
    BLOCK33(expmF,0,0) = I33;
    BLOCK33(expmF,1,1) = I33;
    BLOCK33(expmF,2,2) = I33;
    BLOCK33(expmF,3,3) = I33;
    BLOCK33(expmF,4,4) = I33;
}

if(max_approx_order >= 1){
    // 1st order
    BLOCK33(expmF,0,1) += I33*dt;
    BLOCK33(expmF,1,2) += -RBI_skA*dt;
    BLOCK33(expmF,1,3) += -RBI*dt;
    BLOCK33(expmF,2,2) += -skW*dt;
    BLOCK33(expmF,2,4) += -I33*dt;
}

if(max_approx_order >= 2){
    // 2nd order
    BLOCK33(expmF,0,2) += -RBI_skA*dtm2;
    BLOCK33(expmF,0,3) += -RBI*dtm2;
    BLOCK33(expmF,1,2) += RBI_skA_skW*dtm2;
    BLOCK33(expmF,1,4) += RBI_skA*dtm2;
    BLOCK33(expmF,2,2) += skW2*dtm2;
    BLOCK33(expmF,2,4) += skW*dtm2;
}

if(max_approx_order >= 3){
    // 3rd order
    BLOCK33(expmF,0,2) += RBI_skA_skW*dtm3;
    BLOCK33(expmF,0,4) += RBI_skA*dtm3;
    BLOCK33(expmF,1,2) += -RBI_skA_skW2*dtm3;
    BLOCK33(expmF,1,4) += -RBI_skA_skW*dtm3;
    BLOCK33(expmF,2,2) += -skW3*dtm3;
    BLOCK33(expmF,2,4) += -skW2*dtm3;
}

if(max_approx_order >= 4){
    // 4th order
    BLOCK33(expmF,0,2) += -RBI_skA_skW2*dtm4;
    BLOCK33(expmF,0,4) += -RBI_skA_skW*dtm4;
    BLOCK33(expmF,1,2) += RBI_skA_skW3*dtm4;
    BLOCK33(expmF,1,4) += RBI_skA_skW2*dtm4;
    BLOCK33(expmF,2,2) += skW4*dtm4;
    BLOCK33(expmF,2,4) += skW3*dtm4;
}

if(max_approx_order >=5){
    // 5th order
    BLOCK33(expmF,0,2) += RBI_skA_skW3*dtm5;
    BLOCK33(expmF,0,4) += RBI_skA_skW2*dtm5;
    BLOCK33(expmF,1,2) += -RBI_skA_skW4*dtm5;
    BLOCK33(expmF,1,4) += -RBI_skA_skW3*dtm5;
    BLOCK33(expmF,2,2) += -skW5*dtm5;
    BLOCK33(expmF,2,4) += -skW4*dtm5;
}

};

void ESKF::calcH(const NominalState& X_nom, 
    HMat& H)
{
    H.setZero();
    
    Mat43 mat;
    mat << 0,0,0, 0.5*Mat33::Identity();
    H << I33, O33, O33, O33, O33,
         O43, O43, geometry::q_left_mult(X_nom.q)*mat, O43, O43;
};


void ESKF::test_FMat(const NominalState& X_nom, const Vec3& am, const Vec3& wm,
                    FMat& res)
{
    this->errorStateF(X_nom, am, wm, res);
    std::cout << "FMat:\n";
    std::cout << res << std::endl;
};

void ESKF::test_expm_FMat(const FMat& F, const double& dt, int max_approx_order,
expmFMat& res)
{
    this->expm_FMat(F,dt, max_approx_order, res);
    std::cout << "expm_FMat:\n";
    std::cout << res << std::endl;
};