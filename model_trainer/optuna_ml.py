import argparse
import logging
import optuna
import os
import pandas as pd
from sklearn import preprocessing
from sklearn.ensemble import RandomForestClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.model_selection import train_test_split
from sklearn.neighbors import KNeighborsClassifier
from sklearn.tree import DecisionTreeClassifier
from sklearn.svm import SVC
import xgboost as xgb
import numpy as np
import time

data ={}
model = 'knn'

def parse_args():
    parser = argparse.ArgumentParser(description='Train a machine learning model using Optuna for hyperparameter optimalization.')
    parser.add_argument('--dataset', type=str, required=True,
                        help='Path to the dataset in .h5 or .csv format or dir with .csv files.')
    parser.add_argument('--deploy-path', type=str, required=True,
                        help='Path to save the trained model.')
    parser.add_argument('--name', '-n', type=str, default='TestStudy',
                        help='Name of study')
    return parser.parse_args()

def load_dataset(path):
    if path.endswith('.h5'):
        return pd.read_hdf(path)
    elif path.endswith('.csv'):
        return pd.read_csv(path)
    elif os.path.isdir(path):
        return pd.concat(pd.read_csv(file) for file in os.listdir(path))
    else:
        raise ValueError('Unsupported file format. Please provide a dataset in .h5 or .csv format or path to directory with .csv files')

def objective(trial :optuna.Trial):

    X_train, X_val, y_train, y_val = train_test_split(data.drop(['Label','L7Protocol','ProtocolName'], axis=1), data[['ProtocolName']], test_size=0.2) ## TODO random_state=64      TODO names in parameter

    
    if model == 'knn': #dont work in scikit-learn 1.3.0 works on 1.2.2 see https://github.com/scikit-learn/scikit-learn/issues/26768
        n_neighbors = trial.suggest_int('n_neighbors', 2, 15)
        algo = trial.suggest_categorical('algorithm', ['auto', 'ball_tree', 'kd_tree','brute'])
        weights = trial.suggest_categorical('weights', ['uniform', 'distance'])
        clf = KNeighborsClassifier(n_neighbors=n_neighbors, weights=weights,algorithm=algo) # https://scikit-learn.org/stable/modules/generated/sklearn.neighbors.KNeighborsClassifier.html
    elif model == 'svm':# dont work 
        return 0
        C = trial.suggest_loguniform('C', 1e-10, 1e10)
        kernel = trial.suggest_categorical('kernel', ['linear', 'poly', 'rbf', 'sigmoid'])
        degree = trial.suggest_int('degree', 2, 5)
        gamma = trial.suggest_categorical('gamma', ['scale', 'auto'])
        clf = SVC(C=C, kernel=kernel, degree=degree, gamma=gamma)
    elif model == 'logistic_regression':
        C = trial.suggest_float('C', 1e-10, 1e10,log=True)
        #penalty = trial.suggest_categorical('penalty', ['l1', 'l2'])
        clf = LogisticRegression(C=C, penalty='l2')
    elif model == 'DecisionTreeClassifier':
        max_depth = trial.suggest_int('max_depth', 2, 50)
        criterion = trial.suggest_categorical('criterion',['gini', 'entropy', 'log_loss'])

        clf = DecisionTreeClassifier(max_depth=max_depth,criterion=criterion)
    elif model == 'random_forest':
        n_estimators = trial.suggest_int('n_estimators', 10, 150)       #https://scikit-learn.org/stable/modules/generated/sklearn.ensemble.RandomForestClassifier.html
        max_depth = trial.suggest_int('max_depth', 2, 50)
        criterion = trial.suggest_categorical('criterion',['gini', 'entropy', 'log_loss'])
        clf = RandomForestClassifier(n_estimators=n_estimators, max_depth=max_depth,criterion=criterion)
    else: #xgboost
        #booster=trial.suggest_categorical('booster', ['gbtree', 'gblinear' , 'dart'])   #https://xgboost.readthedocs.io/en/stable/parameter.html#general-parameters
        eta = trial.suggest_float('eta', 1e-8, 1.0,log=True)
        subsample = trial.suggest_float('subsample', 0.5, 1.0)

        max_depth = trial.suggest_int("max_depth", 5, 30)
        gamma = trial.suggest_float('gamma', 1,9)
        reg_alpha = trial.suggest_int('reg_alpha', 0,180)
        reg_lambda = trial.suggest_float('reg_lambda', 0,1)
        colsample_bytree =trial.suggest_float('colsample_bytree', 0, 1)
        min_child_weight = trial.suggest_int('min_child_weight', 0, 20)
        n_estimators = trial.suggest_int('n_estimators', 80/20, 400/20)*20

        clf = xgb.XGBClassifier(objective='binary:logistic',reg_alpha=reg_alpha,reg_lambda=reg_lambda,colsample_bytree=colsample_bytree,min_child_weight=min_child_weight,
                                gamma=gamma,
                                eta=eta,
                                max_depth=max_depth,
                                subsample=subsample)


    
    le = preprocessing.LabelEncoder()
    y_train = le.fit_transform(y_train)
    y_val = le.fit_transform(y_val)
    
    clf.fit(X_train, np.ravel(y_train))

    
    accuracy = clf.score( X_val, np.ravel(y_val))
    return accuracy

if __name__ == '__main__':

    args = parse_args()

    start_time = time.time()

    data = load_dataset(args.dataset)
    print(data.info())

    le = preprocessing.LabelEncoder()

    # Transform the categorical features to numerical values
    data['Flow.ID'] = le.fit_transform(data['Flow.ID'])
    data['Source.IP'] = le.fit_transform(data['Source.IP'])

    data['Destination.IP'] = le.fit_transform(data['Destination.IP'])
    data['Timestamp'] = le.fit_transform(data['Timestamp'])
    data['Label'] = le.fit_transform(data['Label'])
    data['ProtocolName'] = le.fit_transform(data['ProtocolName'])



    logging.basicConfig(level=logging.INFO)

    StudyDict= {}

    methods ={'knn','logistic_regression','DecisionTreeClassifier','random_forest','xgboost'}
    for f in methods:
        #sampler = optuna.samplers.CmaEsSampler() ##TODO sampler
        #sampler =optuna.samplers.TPESampler(multivariate=True)
        study = optuna.create_study(
            storage="sqlite:///db.sqlite3", 
            study_name= args.name + '_' + f,
            direction="maximize",
            #sampler=sampler,
            load_if_exists=True,
        )
        model =f
        study.optimize(objective, n_trials=50,timeout=240,n_jobs=-1)#TODO better timeout
        StudyDict[f] = study
        
    

    BestModel =''
    BestScore=0
    for f in methods:
        if StudyDict[f].best_value > BestScore:
            BestModel=f
            BestScore=StudyDict[f].best_value

    model=BestModel
    StudyDict[BestModel].optimize(objective, n_trials=50)

    end_time = time.time()


    logging.info(f'Best study: {BestModel}')
    logging.info(f'Best hyperparameters: {StudyDict[BestModel].best_params}')
    logging.info(f'Best accuracy: {StudyDict[BestModel].best_value}')
    logging.info(f'Time taken: {end_time - start_time:.2f} seconds')

    ##TODO train and export model

